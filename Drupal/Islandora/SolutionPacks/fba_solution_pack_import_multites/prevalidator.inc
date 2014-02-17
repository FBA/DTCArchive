<?php

/**
 * Implements hook_form().
 *
 * Landing page for import function
 *
 * @return string
 *   html formatted output
 */
function fba_solution_pack_import_multites_form_prevalidator(&$form_state = array()) {

  $form = array();
  // Set the form encoding type.
  $form['#attributes']['enctype'] = "multipart/form-data";

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Batch preprocess /tmp/multites.xml file'),
  );

  return $form;
}

/** RDF Relations section * */

/**
 * Implements hook_submit.
 * Return status message of autocomplete path and title strings.
 *
 * @param array $form
 *   standard form variable for a hook
 * @param array $form_state
 *   standard form state variable for a hook
 */
function fba_solution_pack_import_multites_form_prevalidator_submit($form, &$form_state) {
  if (!$form_state['form_has_errors']) {

    $batch = array(
      'title' => t('Validate and process MultiTes export file'),
      'operations' => array(), // Operations to complete, in order. Defined below.
      'finished' => '_tes_prevalidator_batch_finished', // Last function to call.
      'init_message' => t('Initialising...'),
      'progress_message' => t('Processed @current out of @total.'),
      'error_message' => t('MultiTes export file to xml validation encountered an error.'),
    );
    //clear out the files from previous batch run
    $invalidtextfile = fopen('/tmp/multitesinvalid.csv', "w+");
    fclose($invalidtextfile);
    $fedoratnrfile = fopen('/tmp/multitesfedoratnr.txt', "w+");
    fclose($fedoratnrfile);

    //empty the temporary database table
    $queryresult = db_query('DELETE FROM {%s}', TEMP_PID_TNR_AUTHVAR_TABLE);


    //create the separate concept object xml files
    $batch['operations'][] = array('_tes_prevalidator_batch_first', array());

    //need to lookup pid and tnr, in an array in this case
    $allpids = _fba_get_pids_from_collections();    //all the current pids in fedora commons
    foreach ($allpids as $vocabpid) {
      //create batch operation for each
      $batch['operations'][] = array('_tes_prevalidator_batch_second', array($vocabpid));
    }
    // Tip the first domino.
    batch_set($batch);
  }
}

/**
 *
 * @param type $form
 * @param type $form_state
 */
function fba_solution_pack_import_multites_form_prevalidator_validate($form, &$form_state) {
  $xmlFileToLoad = '/tmp/multites.xml';
  $form_state['form_has_errors'] = FALSE;

  if (!file_exists($xmlFileToLoad)) {
    $form_state['form_has_errors'] = TRUE;
    form_set_error('', t($xmlFileToLoad . ' cannot be opened.'));
  }
}

function _tes_prevalidator_batch_first(&$context) {
  $xmlfilename = '/tmp/multites.xml';
  $validxmlfileprefix = '/tmp/FBAtes';
  $invalidtextfile = fopen('/tmp/multitesinvalid.csv', "a+"); //append
  $tnrlistfile = fopen('/tmp/FBAtnr.txt', "w+");

  if ($invalidtextfile && $tnrlistfile) {

    //Process the XML file
    //use XMLReader as this doesn't drag the whole file in at once
    $xmlrdr = new XMLReader();

    if ($xmlrdr->open(trim($xmlfilename))) {
      $xmlreadok = $xmlrdr->read();
      while ($xmlreadok) {
        $nodetype = $xmlrdr->nodeType;
        switch ($nodetype) {
          case XMLReader::ELEMENT: {
              $name = $xmlrdr->name;
              if ($name == 'CONCEPT') {
                //handle the concept
                $domrecordelement = $xmlrdr->expand();
                if ($domrecordelement) {
                  $xml = new DOMDocument;
                  $node = $xml->importNode($domrecordelement, true);
                  $xml->appendChild($node);

                  $recordreturntext = _tes_validate_concept_node($xml);
                  if ($recordreturntext == FALSE) {
                    //No validation errors so put the concept into the valid file
                    $c14n = $xml->C14N();
                    $tnr = $xml->getElementsByTagName('TNR')->item(0)->nodeValue;
                    fwrite($tnrlistfile, $tnr . "\n");
                    $validxmlfile = fopen($validxmlfileprefix . $tnr . '.xml', "w+");
                    fwrite($validxmlfile, $c14n);
                    fclose($validxmlfile);
                  }
                  else {
                    //report the error(s)
                    fwrite($invalidtextfile, $recordreturntext);
                  }
                }
                else {
                  $current_set = &_batch_current_set();
                  $current_set['success'] = FALSE;
                  $context['results'][] = 'Batch 1: XML Expand Failed.';
                  $context['finished'] = 1.0;
                  $returnvalue = FALSE;
                }
              }
              break;
            }
        }
        $recordreturntext = '';
        $xmlreadok = $xmlrdr->read();
      }
      $xmlrdr->close();

      fclose($invalidtextfile);
      fclose($tnrlistfile);

      $returnvalue = TRUE;
    }
    else {
      $current_set = &_batch_current_set();
      $current_set['success'] = FALSE;
      $context['results'][] = 'Batch 1: Cannot open multites.xml.';
      $context['finished'] = 1.0;
      $returnvalue = FALSE;
    }
  }
  else {
    //failure to create the output files
    $current_set = &_batch_current_set();
    $current_set['success'] = FALSE;
    $context['results'][] = 'Batch 1: Cannot create output files.';
    $context['finished'] = 1.0;
    $returnvalue = FALSE;
  }

  return $returnvalue;
}

function _tes_prevalidator_batch_second($pid, &$context) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');

  $fedoraobject = new Fedora_Item($pid);

  $mads = $fedoraobject->get_datastream_dissemination('MADS');

  if (!$mads) {
    $invalidtextfile = fopen('/tmp/multitesinvalid.csv', "a+");
    fwrite($invalidtextfile, ', Object with pid - ' . $pid . ' has no MADS datastream');
    fclose($invalidtextfile);
  }
  else {
    $simplemads = simplexml_load_string($mads);

    $madsidattrs = $simplemads->identifier->attributes();
    $madsidtype = (string) $madsidattrs['type'];

    if (strcmp($madsidtype, 'TNR') == 0) {
      $madsidentifierval = (string) $simplemads->identifier;
      $fedoratnrfile = fopen('/tmp/multitesfedoratnr.txt', "a+");
      fwrite($fedoratnrfile, $madsidentifierval . "\n");
      fclose($fedoratnrfile);
    }
    else {
      $madsidentifierval = NULL;
    }

    $madsvariant = (string) $simplemads->variant->topic;
    $madsauthority = (string) $simplemads->authority->topic;

    if ($madsvariant) {
      _tes_db_populate($pid, $madsidentifierval, $madsvariant);
    }
    else {
      _tes_db_populate($pid, $madsidentifierval, $madsauthority);
    }

    $fedoraobject->forget();
  }
}

/**
 *
 * @param type $success
 * @param type $results
 * @param type $operations
 */
function _tes_prevalidator_batch_finished($success, $results, $operations) {
  $filenamesuffix = 'multitesinvalid.csv';
  $invalidtextfile = '/tmp/' . $filenamesuffix;
  $sortedinvalidtextfile = '/tmp/sorted' . $filenamesuffix;
  $tnrlistfile = '/tmp/FBAtnr.txt';

  $lines = file($invalidtextfile);
  natsort($lines);
  $arraywithheadings = 'TNR, Term, Message' . "\n" . implode("", $lines);
  file_put_contents($sortedinvalidtextfile, $arraywithheadings);

  $lines = file($tnrlistfile);
  natsort($lines);
  file_put_contents('/tmp/FBAsortedtnr.txt', implode("", $lines));

  if ($success) {
    $message = t('Prevalidator finished.');
  }
  else {
    $message = t('Prevalidator finished with error - ' . $results[0]);
  }
  drupal_set_message($message);
}

/**
 *
 * @param type $xml
 * @return string|boolean
 */
function _tes_validate_concept_node($xml) {
  $recordhaserror = FALSE;
  $returnpagetext = '';

  //Check mandatory fields
  $descriptor = $xml->getElementsByTagName('DESCRIPTOR')->item(0)->nodeValue;
  $nondescriptor = $xml->getElementsByTagName('NON-DESCRIPTOR')->item(0)->nodeValue;

  if ((is_null($descriptor)) && (is_null($nondescriptor))) {
    $returnpagetext .= 'Descriptor and NonDescriptor both null' . "\n";
    $recordhaserror = TRUE;
  }

  $errortext = _check_null_or_incorrect_count($xml, 'UPD');
  if (!is_null($errortext)) {
    $returnpagetext .= $errortext;
    $recordhaserror = TRUE;
  }

  $errortext = _check_null_or_incorrect_count($xml, 'INP');
  if (!is_null($errortext)) {
    $returnpagetext .= $errortext;
    $recordhaserror = TRUE;
  }

//this is possibly a repeating field
//  $app = $xml->getElementsByTagName('APP')->item(0)->nodeValue;
//  if (is_null($app)) {
//    $returnpagetext .= 'APP is null' . "\n";
//    $recordhaserror = TRUE;
//  }
  $errortext = _check_null_or_incorrect_count($xml, 'TNR');
  if (!is_null($errortext)) {
    $returnpagetext .= $errortext;
    $recordhaserror = TRUE;
    //put name in here, so error message can be traced
    $tnr = is_null($descriptor) ? $nondescriptor : $descriptor;
  }
  else {
    $tnr = $xml->getElementsByTagName('TNR')->item(0)->nodeValue;
  }


  $sd = $xml->getElementsByTagName('SD')->item(0)->nodeValue;
  $df = $xml->getElementsByTagName('DF')->item(0)->nodeValue;

  if (is_null($sd)) {
    if (is_null($df)) {
      //both are null: this is ok
    }
    else {
      //df has value, sd doesn't
      $returnpagetext .= 'DF found SD missing' . "\n";
      $recordhaserror = TRUE;
    }
  }
  else {
    //sd has value
    if (is_null($df)) {
      //df doesn't
      $returnpagetext .= 'DF missing SD found' . "\n";
      $recordhaserror = TRUE;
    }
  }

  $sc = $xml->getElementsByTagName('SC')->item(0)->nodeValue;

  if (!is_null($sc)) {
    //SC has a value
    if ($sc != 'GEO') {
      //GEO is the only valid value
      $returnpagetext .= 'SC has invalid value => ' . $sc . "\n";
      $recordhaserror = TRUE;
    }
  }


  if ($recordhaserror) {
    $returnpage = $tnr . ',';
    $authvar = is_null($descriptor) ? $nondescriptor : $descriptor;
    $returnpage .= $authvar . ',';
    $returnpage .= $returnpagetext; // . "\n";
    return $returnpage;
  }
  else {
    return FALSE;
  }
}

/**
 *
 * @param type $xml
 * @param type $tagtext
 * @return string
 */
function _check_null_or_incorrect_count($xml, $tagtext) {
  $returnpagetext = NULL;
  $tag = $xml->getElementsByTagName($tagtext);
  if ($tag->length != 1) {
    $returnpagetext .= 'incorrect number of ' . $tagtext . ' tags' . "\n";
    //$recordhaserror = TRUE;
  }
  else {
    $value = $tag->item(0)->nodeValue;
    if (is_null($value)) {
      $returnpagetext .= $tagtext . ' is null' . "\n";
      //$recordhaserror = TRUE;
    }
  }
  return $returnpagetext;
}