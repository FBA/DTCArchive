<?php

/**
 * Implements hook_form().
 *
 * Landing page for import function
 *
 * @return string
 *   html formatted output
 */
function fba_solution_pack_import_multites_form_ingest(&$form_state = array()) {

  $form = array();
// Set the form encoding type.
  $form['#attributes']['enctype'] = "multipart/form-data";

// Add a file upload file.
  $form['ingest-file-location'] = array(
    '#type' => 'file',
    '#title' => t('Upload MultiTes XML file'),
    '#description' => t('Warning, this will overwrite all matching vocab objects.'),
  );

// Add a submit button.
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Process XML File'),
  );

  return $form;
}

/**
 * Implements hook_submit.
 * Return status message of autocomplete path and title strings.
 *
 * @param array $form
 *   standard form variable for a hook
 * @param array $form_state
 *   standard form state variable for a hook
 */
function fba_solution_pack_import_multites_form_ingest_submit($form, &$form_state) {

  module_load_include('inc', 'custom_search', 'includes/apachesolr_search');
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');

  $path = drupal_get_path('module', 'fba_solution_pack_import_multites');
  $file = $form_state['values']['file'];

  try {
    $xml = new DOMDocument();
    $xml->load($file->filepath);

    $xsl = new DOMDocument();
    $xsl->load($path . '/xslt/convertTEStoMADS.xsl');

    $proc = new XSLTProcessor();
    $proc->importStylesheet($xsl);
    $doc = $proc->transformToDoc($xml);

    $elements = $doc->getElementsByTagName('mads');

    $totalNewObjects = 0;
    $totalUpdateObjects = 0;
    $totalErrors = 0;
    $totalCount = 0;

// Pass number 1, just add the new items, or update the existing ones
// Next pass (further down) will check/add all the RDF links

    foreach ($elements as $node) {
//the node is the mads record from the xml
      $totalCount++;

      $authority = $node->getElementsByTagName('authority')->item(0)->nodeValue;
      $variant = $node->getElementsByTagName('variant')->item(0)->nodeValue;
      $identifier = $node->getElementsByTagName('identifier')->item(0)->nodeValue;

      if (is_null($identifier) || empty($identifier)) {
// Object doesn't have a TNR, so output an error message
        $totalErrors++;
        if (is_null($authority) || empty($authority)) {
          watchdog('content', 'Variant: %variant has invalid TNR.', array('%variant' => $variant));
        }
        else {
          watchdog('content', 'Authority: %auth has invalid TNR.', array('%auth' => $authority));
        }
      }
      else {
//try and find a fedora object with this TNR
//do solr search for eg mads.identifier:5
        $solrresult = _perform_solr_search('mads.identifier:' . $identifier);
        $rawresponse = $solrresult->getRawResponse();
        $responsearray = json_decode($rawresponse, TRUE);
        $total = (int) $solrresult->response->numFound;

// Need to change date formats
        $UTCdateformatstring = 'Y-m-d\TH:i:s'; // need to append '.\000Z'; 
//recordCreationDate
        $recordcreationdate = $node->getElementsByTagName('recordCreationDate');
        $reccredatevalue = $recordcreationdate->item(0)->nodeValue;
        $reccreDateTime = new DateTime($reccredatevalue);
//Translate to YYYY-MM-DDTHH:MM:SS.uuuZ
        $reccresolrDateTime = $reccreDateTime->format($UTCdateformatstring) . '.000Z';
        $recordcreationdate->item(0)->nodeValue = $reccresolrDateTime;

//recordChangeDate
        $recordchangedate = $node->getElementsByTagName('recordChangeDate');
        $recchgdatevalue = $recordchangedate->item(0)->nodeValue;
        $stdDateTime = new DateTime($recchgdatevalue);
//Translate to YYYY-MM-DDTHH:MM:SS.FFFZ
        $solrDateTime = $stdDateTime->format($UTCdateformatstring) . '.000Z';
        $recordchangedate->item(0)->nodeValue = $solrDateTime;

//approved date
        $notes = $node->getElementsByTagName('note');
        foreach ($notes as $note) {
          $ntype = $note->getAttribute('type');
          if ($ntype == 'approved date') {
            $nval = $note->nodeValue;
            $stdDateTime = new DateTime($nval);
//Translate to YYYY-MM-DDTHH:MM:SS.FFFZ
            $solrDateTime = $stdDateTime->format($UTCdateformatstring) . '.000Z';
            $note->nodeValue = $solrDateTime;
          }
        }

        if ($total == 0) {
          //couldn't find an object with that TNR
          $totalNewObjects++;

          // add this object to fedora via the vocab content model
          $namespace = 'vocab';
          $collection_pid = 'info:fedora/' . $namespace . ':collection';
          $nextavailablepid = fedora_item::get_next_PID_in_namespace($namespace);
          global $user;
          if ($variant == NULL) {
            $item = fedora_item::ingest_new_item($nextavailablepid, 'A', $authority, $user->name);
          }
          else {
            $item = fedora_item::ingest_new_item($nextavailablepid, 'A', $variant, $user->name);
          }
          $item->add_relationship('isMemberOfCollection', $collection_pid, RELS_EXT_URI);
          $item->add_relationship('hasModel', 'fba:vocab_cmodel', FEDORA_MODEL_URI);
          $thumbnail = $path . '/images/vocabicon.png';
          $item->add_datastream_from_file($thumbnail, 'TN');


//get the mads object as a string here
          $madsnodexmlstring = $node->ownerDocument->saveXML($node);

// force the namespaces into the <mads> tag
//TODO: The mads string replacement below isn't being written to the object!
          $madsstring = str_replace('<mads>', '<mads xmlns="http://www.loc.gov/mads/v2" xmlns:xlink="http://www.w3.org/1999/xlink">', $madsnodexmlstring);
          $dsid = 'MADS';

//need to convert the date formats from 2013-03-12 to 2013-03-12T00:00:00.000Z

          $item->add_datastream_from_string($madsstring, $dsid, 'MADS Record', 'text/xml', 'X');
        }
        else {
// Replace the MADS record in the existing object with the 
// mads record from the xml
          $totalUpdateObjects++;

          $responsedocs = $responsearray['response']['docs'];
          $pid = $responsedocs[0][PID];

//get the mads object as a string here
          $madsnodexmlstring = $node->ownerDocument->saveXML($node);

// force the namespaces into the <mads> tag
          $madsstring = str_replace('<mads>', '<mads xmlns="http://www.loc.gov/mads/v2" xmlns:xlink="http://www.w3.org/1999/xlink">', $madsnodexmlstring);

          $item = new fedora_item($pid);
          $returnvalue = $item->modify_datastream_by_value($madsstring, 'MADS', 'MADS Record', 'text/xml');
// check $returnvalue for being ok?

          $thumbnail = $path . '/images/vocabicon.png';
          $datastreams = $item->get_datastreams_list_as_array();
          if (array_key_exists('TN', $datastreams)) {
            $item->purge_datastream('TN');
          }
          $item->add_datastream_from_file($thumbnail, 'TN');
        }
      }
    }
    drupal_set_message(t('Total:' . $totalCount . '; New:' . $totalNewObjects . '; Updates:' . $totalUpdateObjects . '; Errors (see watchdog):' . $totalErrors));
  } catch (Exception $e) {
    form_set_error(NULL, $e->getMessage());
  }
}

function fba_solution_pack_import_multites_form_ingest_validate($form, &$form_state) {
  $fieldname = 'ingest-file-location';

// If a file was uploaded, process it.
  if (isset($_FILES['files']) && is_uploaded_file($_FILES['files']['tmp_name'][$fieldname])) {

// Attempt to save the uploaded file to a temp area on disk.
    $file = file_save_upload($fieldname);

// set error if file was not uploaded
    if (!$file) {
      form_set_error($fieldname, 'Error uploading file.');
      return;
    }

    $filenameparts = pathinfo($file->filename);

    if (strtolower($filenameparts['extension']) != 'xml') {
      form_set_error($fieldname, 'File extension must be xml.');
    }
    else {
// TODO: check if it's a well-formed xml file?
//
// Set files to form_state, to process when form is submitted.
      $form_state['values']['file'] = $file;
    }
  }
  else {
// Set error.
    form_set_error($fieldname, 'Error uploading file.');
    return;
  }
}