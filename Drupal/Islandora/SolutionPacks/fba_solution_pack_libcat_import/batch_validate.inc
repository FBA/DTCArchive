<?php

function fba_solution_pack_libcat_import_form_batch_validator(&$form_state = array()) {
  $form = array();
// Set the form encoding type.
  $form['#attributes'] = array('enctype' => "multipart/form-data");

// Add a submit button.
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Batch validate /tmp/FBArecords*.xml files'),
  );

  return $form;
}

/**
 * Implements hook_submit.
 *
 * @param array $form
 *   standard form variable for a hook
 * @param array $form_state
 *   standard form state variable for a hook
 */
function fba_solution_pack_libcat_import_form_batch_validator_submit($form, &$form_state) {
  if (!$form_state['form_has_errors']) {
    $batch = array(
      'title' => t('Validate and process CDSisis export file'),
      'operations' => array(), // Operations to complete, in order. Defined below.
      'finished' => '_validator_batch_finished', // Last function to call.
      'init_message' => t('Initialising...'),
      'progress_message' => t('Processed @current out of @total.'),
      'error_message' => t('CDSisis export file to xml validation encountered an error.'),
    );
    //clear out the files from previous batch runs and write header text where appropriate
    $invalidtextfile = fopen('/tmp/FBAinvalidtext.csv', "w+");
    $validobjectsfile1 = fopen('/tmp/FBAvalidobjects.xml', "w+");
    $mfnlistfile1 = fopen('/tmp/FBAmfn.txt', "w+"); //clears file

    fwrite($invalidtextfile, 'MFN, Message' . "\n"); //Headings for csv file columns
    fwrite($validobjectsfile1, '<?xml version="1.0" encoding="ISO-8859-1"?>' . "\n"); //Header for XML file
    fwrite($validobjectsfile1, '<DATABASE_FBA>' . "\n"); //Header for XML file 

    fclose($invalidtextfile);
    fclose($validobjectsfile1);
    fclose($mfnlistfile1);

    $listofxmlfiles = file('/tmp/FBArecfilelist.txt');

    foreach ($listofxmlfiles as $xmlfile) {
      $batch['operations'][] = array('_validator_batch_first', array($xmlfile));
    }

    // Tip the first domino.
    batch_set($batch);
  }
}

function fba_solution_pack_libcat_import_form_batch_validator_validate($form, &$form_state) {
  $xmlFileToLoad = '/tmp/FBAmain.xml';
  $form_state['form_has_errors'] = FALSE;

  if (!file_exists($xmlFileToLoad)) {
    $form_state['form_has_errors'] = TRUE;
    form_set_error('', t($xmlFileToLoad . ' cannot be opened.'));
  }
}

function _validator_batch_first($xmlfilename, &$context) {
  //drupal_set_message('Fired field validation operation.');
  $validobjectsfile = fopen('/tmp/FBAvalidobjects.xml', "a+"); //append
  $invalidtextfile = fopen('/tmp/FBAinvalidtext.csv', "a+"); //append
  $mfnlistfile = fopen('/tmp/FBAmfn.txt', "a+"); //append

  if ($validobjectsfile && $invalidtextfile && $mfnlistfile) {

    //Process the XML file
    //use XMLReader as this doesn't drag the whole file in at once
    $xmlrdr = new XMLReader();

    if ($xmlrdr->open(trim($xmlfilename))) {
      $xmlreadok = $xmlrdr->read();
      while ($xmlreadok) {
        //Ignore lines between "<?xml etcetc" and "<DATABASE_FBA>"
        $nodetype = $xmlrdr->nodeType;
        switch ($nodetype) {
          case XMLReader::ELEMENT: {
              $name = $xmlrdr->name; //this brings back, amongst others RECORD and DATABASE_FBA!
              if ($name == 'RECORD') {
                //handle the record
                $domrecordelement = $xmlrdr->expand();
                if ($domrecordelement) {
                  $xml = new DOMDocument;
                  $node = $xml->importNode($domrecordelement, true);
                  $xml->appendChild($node);

                  //DEBUG line below so I can view the xml in text format!
                  //$outputtest = $xml->saveXML();
                  //write all mfn's to $mfnlistfile
                  $mfn = $xml->getElementsByTagName('Mfn')->item(0)->nodeValue;

                  //ignore mfn 1 as that's always a title record in cdsisis
                  if ($mfn > 1) {
                    fwrite($mfnlistfile, $mfn . "\n");

                    $recordreturntext = _validate_record_node($xml);

                    if ($recordreturntext == FALSE) {
                      $c14n = $xml->C14N();
                      fwrite($validobjectsfile, $c14n);
                    }
                    else {
                      fwrite($invalidtextfile, $recordreturntext);
                    }
                  }
                }
                else {
                  $current_set = &_batch_current_set();
                  $current_set['success'] = FALSE;
                  $context['results'][] = 'Batch 2: XML Expand Failed.';
                  $context['finished'] = 1.0;
                  $returnvalue = FALSE;
                }
              }
              break;
            }
        }
        if ($recordreturntext) {
          //some sort of error found
          $pagereturntext .= $recordreturntext;
        }
        $recordreturntext = '';
        $xmlreadok = $xmlrdr->read();
      }
      $xmlrdr->close();

      $results_file = fopen("/tmp/resultunsorted.txt", "w+");

      if ($results_file) {
        fwrite($results_file, $pagereturntext);
        fclose($results_file);
      }
      fclose($validobjectsfile);
      fclose($invalidtextfile);
      fclose($mfnlistfile);
      $returnvalue = TRUE;
    }
    else {
      //couldn't open the postsed2file
      $current_set = &_batch_current_set();
      $current_set['success'] = FALSE;
      $context['results'][] = 'Batch 2: Cannot open postsed2file.';
      $context['finished'] = 1.0;
      $returnvalue = FALSE;
    }
  }
  else {
    //failure to create the output files
    $current_set = &_batch_current_set();
    $current_set['success'] = FALSE;
    $context['results'][] = 'Batch2: Cannot create output files.';
    $context['finished'] = 1.0;
    $returnvalue = FALSE;
  }

  return $returnvalue;
}

function _validator_batch_finished($success, $results, $operations) {
  //write the footer
  $validobjectsfile2 = fopen('/tmp/FBAvalidobjects.xml', "a+");
  fwrite($validobjectsfile2, '</DATABASE_FBA>' . "\n"); //Footer for XML file 
  fclose($validobjectsfile2);
  if ($success) {
    $message = t('All CDSisis records processed.');
  }
  else {
    $message = t('Finished with error - ' . $results[0]);
  }
  drupal_set_message($message);
}