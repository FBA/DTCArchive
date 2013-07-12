<?php

function fba_solution_pack_libcat_import_form_batch_preprocessor(&$form_state = array()) {
  $form = array();
// Set the form encoding type.
  $form['#attributes'] = array('enctype' => "multipart/form-data");

// Add a submit button.
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Batch preprocess /tmp/FBAmain.xml file'),
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
function fba_solution_pack_libcat_import_form_batch_preprocessor_submit($form, &$form_state) {
  if (!$form_state['form_has_errors']) {
    $batch = array(
      'title' => t('Preprocess CDSisis export file'),
      'operations' => array(), // Operations to complete, in order. Defined below.
      'finished' => '_preprocessor_batch_finished', // Last function to call.
      'init_message' => t('Initialising...'),
      'progress_message' => t('Processed @current out of @total.'),
      'error_message' => t('CDSisis export file to xml preprocessing encountered an error.'),
    );

    $batch['operations'][] = array('_preprocessor_batch_first', array());
    $xmllistoffiles = fopen('/tmp/FBArecfilelist.txt', "w+");
    $numrecsperfile = 5000;
    $biggestmfn = 260000;
    for ($i = 0; $i < $biggestmfn; $i = $i + $numrecsperfile) {
      $xmlfilename = '/tmp/FBArecords' . $i . '.xml';
      fwrite($xmllistoffiles, $xmlfilename . "\n");
      $batch['operations'][] = array('_preprocessor_batch_second', array($i, $i + $numrecsperfile - 1, $xmlfilename));
    }
    fclose($xmllistoffiles); //TODO: check, need to delete this at the end of importing, or maybe empty it at the start of batch processing here.

    // Tip the first domino.
    batch_set($batch);
  }
}

function fba_solution_pack_libcat_import_form_batch_preprocessor_validate($form, &$form_state) {
  $xmlFileToLoad = '/tmp/FBAmain.xml';
  $form_state['form_has_errors'] = FALSE;

  if (!file_exists($xmlFileToLoad)) {
    $form_state['form_has_errors'] = TRUE;
    form_set_error('', t($xmlFileToLoad . ' cannot be opened.'));
  }
}

function _preprocessor_batch_first(&$context) {
  drupal_set_message('Fired xml text replacement batch operation.');

  $xmlFileToLoad = '/tmp/FBAmain.xml';
  $postsed1file = '/tmp/FBApostsed1.xml';
  $postsed2file = '/tmp/FBApostsed2.xml';
  $sedretcode1 = 1;
  $sedretcode2 = 1;

  //Replace "RECORD;" with "RECORD"
  $sedcmd = 'sed \'s/RECORD;/RECORD/g\' ' . $xmlFileToLoad . ' > ' . $postsed1file;
  passthru($sedcmd, $sedretcode1);
  //replace & with &amp;
  $sedcmd = 'sed \'s/&/&amp;/g\' ' . $postsed1file . ' > ' . $postsed2file;
  passthru($sedcmd, $sedretcode2);

  //If $sedretcode isn't 0, something hasn't worked with the sed command
  if (($sedretcode1 == 0) && ($sedretcode2 == 0)) {
    //$current_set = &_batch_current_set();
    //$current_set['success'] = TRUE;
    //$context['results'][] = 'Batch 1: Finished.';
    //$context['finished'] = 1.0;
    $returnvalue = TRUE;
  }
  else {
    $current_set = &_batch_current_set();
    $current_set['success'] = FALSE;
    $context['results'][] = 'Batch 1: sed commands failed rc1 =' . $sedretcode1 . '. rc2 = ' . $sedretcode2 . '.';
    $context['finished'] = 1.0;
    $returnvalue = FALSE;
  }

  //delete the surplus file out of the /tmp folder
  unlink($postsed1file);
  return $returnvalue;
}

function _preprocessor_batch_second($lowmfn, $himfn, $xmlfilename, &$context) {
//  drupal_set_message('Fired file split operation.');
  $postsed2file = '/tmp/FBApostsed2.xml';
  $smallerrecordsfile = fopen($xmlfilename, "w+");

  if ($smallerrecordsfile) {
    fwrite($smallerrecordsfile, '<?xml version="1.0" encoding="ISO-8859-1"?>' . "\n"); //Header for XML file
    fwrite($smallerrecordsfile, '<DATABASE_FBA>' . "\n"); //Header for XML file 
    //Process the XML file
    //use XMLReader as this doesn't drag the whole file in at once
    $xmlrdr = new XMLReader();
    $filefullywritten = FALSE;

    if ($xmlrdr->open($postsed2file)) {
      //read the first record to prime the while loop
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

                  $mfn = $xml->getElementsByTagName('Mfn')->item(0)->nodeValue;

                  //ignore mfn 1 as that's always a title record in cdsisis
                  if ($mfn > 1) {
                    if ($mfn < $lowmfn) {
                      //skip and read next record as we haven't reached lowest mfn yet
                      break;
                    }
                    else {
                      if ($mfn <= $himfn) {
                        $c14n = $xml->C14N();
                        fwrite($smallerrecordsfile, $c14n);
                      }
                      else {
                        //we've reached the maximum mfn for this file
                        $filefullywritten = TRUE;
                        break;
                      }
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
        if ($filefullywritten == TRUE) {
          $xmlreadok = FALSE;
        }
        else {
          $xmlreadok = $xmlrdr->read();
        }
      }
      $xmlrdr->close();

      fwrite($smallerrecordsfile, '</DATABASE_FBA>' . "\n"); //Footer for XML file 

      fclose($smallerrecordsfile);
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
}

function _preprocessor_batch_finished($success, $results, $operations) {
  if ($success) {
    $message = t('All CDSisis records processed.');
  }
  else {
    $message = t('Finished with error - ' . $results[0]);
  }
  drupal_set_message($message);
}