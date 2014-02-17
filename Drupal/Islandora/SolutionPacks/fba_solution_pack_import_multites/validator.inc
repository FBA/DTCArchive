<?php

/**
 * Implements hook_form().
 *
 * Landing page for import function
 *
 * @return string
 *   html formatted output
 */
function fba_solution_pack_import_multites_form_validator(&$form_state = array()) {

  $form = array();
// Set the form encoding type.
  $form['#attributes']['enctype'] = "multipart/form-data";

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Validate rdfs for all vocab objects'),
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
function fba_solution_pack_import_multites_form_validator_submit($form, &$form_state) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_collection');

  if (!$form_state['form_has_errors']) {
    $batch = array(
      'title' => t('Process RDF validator for Multites vocab/geo objects'),
      'operations' => array(), // Operations to complete, in order. Defined below.
      'finished' => '_tes_validator_batch_finished', // Last function to call.
      'init_message' => t('Initialising...'),
      'progress_message' => t('Processed @current out of @total.'),
      'error_message' => t('Multites vocab validation check encountered an error.'),
    );
    //$debugtextfile = fopen('/tmp/allrelations.csv', "w+");
    //fclose($debugtextfile);
    //clear out the files from previous batch runs and write header text where appropriate
    $invalidtextfile = fopen('/tmp/multitespostvalidate.csv', "w+");
    fclose($invalidtextfile);
    $all_pids = _fba_get_pids_from_collections();
//    $member_pids_vocab = get_related_items_as_array('vocab:collection', 'isMemberOfCollection', 1000000);
//    $member_pids_geo = get_related_items_as_array('geo:collection', 'isMemberOfCollection', 1000000);
    //limit is set to 1 million as there's no "all" value for this function
    foreach ($all_pids as $pid) {
      $batch['operations'][] = array('_tes_validator_batch_first', array($pid));
    }
    // Tip the first domino.
    _tes_import_batch_process_pages($batch);
  }
}

/**
 *
 * @param type $memberpid
 * @param type $context
 * @return boolean
 */
function _tes_validator_batch_first($memberpid, &$context) {
  module_load_include('inc', 'custom_search', 'includes/apachesolr_search');
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'api/fedora_collection');
  module_load_include('inc', 'fedora_repository', 'api/dublin_core');

  //check if this is the first one, and zero all the statistics
  if (!isset($context['results']['preferredterms'])) {
    $context['results']['preferredterms'] = 0;
    $context['results']['nonpreferredterms'] = 0;
    $context['results']['topterms'] = 0;
    $context['results']['toptermsarray'] = array();
    $context['results']['orphanterms'] = 0;
    $context['results']['orphantermsarray'] = array();
    $context['results']['hierrels'] = 0;
    $context['results']['equivrels'] = 0;
    $context['results']['assocrels'] = 0;
    $context['results']['georels'] = 0;
    $context['results']['geoterms'] = 0;
    $context['results']['latestupd'] = new DateTime("1980-01-01T00:00:00.000Z"); //arbritrary value just to get things started
  }

  //some arrays for statistics
  $hierarchicalbt = array('hasBroaderAuthority');
  $hierarchicalnt = array('hasNarrowerAuthority');
  $equivalence = array('useFor', 'useInstead');
  $associative = array('hasRelatedAuthority');
  $geographic = array('hasGeographicFeature', 'isGeographicFeatureOf');
  //end of statistics arrays
  //$debugtextfile = fopen('/tmp/allrelations.csv', "a+");

  $invalidtextfile = fopen('/tmp/multitespostvalidate.csv', "a+");
  if ($invalidtextfile) {

    $rel_inverse["useInstead"] = "useFor";
    $rel_inverse["useFor"] = "useInstead";
    $rel_inverse["hasBroaderAuthority"] = "hasNarrowerAuthority";
    $rel_inverse["hasNarrowerAuthority"] = "hasBroaderAuthority";
    $rel_inverse["hasRelatedAuthority"] = "hasRelatedAuthority";
    $rel_inverse["hasGeographicFeature"] = "isGeographicFeatureOf";
    $rel_inverse["isGeographicFeatureOf"] = "hasGeographicFeature";

    // get the mads datastream for this object
    $item = new fedora_item($memberpid);

    $pidnamespace = substr($memberpid, 0, strpos($memberpid, ':'));
    if (strcmp($pidnamespace, 'geo') == 0) {
      $context['results']['geoterms']++;
    }

    $relationships = array();
    $relsext = $item->get_datastream_dissemination('RELS-EXT');
    $mads = $item->get_datastream_dissemination('MADS');
    $simplemads = simplexml_load_string($mads);
    $madsvariant = $simplemads->variant->topic;
    $madsauthority = $simplemads->authority->topic;
    $upd = new DateTime($simplemads->recordInfo->recordChangeDate);
    if ($upd > $context['results']['latestupd']) {
      $context['results']['latestupd'] = $upd;
    } //This will show the last edit done in multites

    $authvar = '';
    if (!is_null($madsvariant)) {
      //it's a non preferred term
      $context['results']['nonpreferredterms']++;
      $authvar = (string) $madsvariant;
    }
    if (!is_null($madsauthority)) {
      //it's a preferred term
      $context['results']['preferredterms']++;
      $authvar = (string) $madsauthority;
    }

    // Parse the RELS-EXT into an associative array.
    $relsextxml = new DOMDocument();
    $relsextxml->loadXML($relsext);
    $relsextxml->normalizeDocument();

    $allTags = array();
    $allTags[] = $relsextxml->getElementsByTagNameNS('*', '*');

    foreach ($allTags as $tags) {
      foreach ($tags as $child) {
        $attns = $child->getAttributeNS('http://www.w3.org/1999/02/22-rdf-syntax-ns#', 'resource');
        if ($attns != NULL) {
          $value = preg_replace('/info:fedora\//', '', $attns);

          $relationships[$child->tagName][] = $value;
        }
      }
    }

    $countforbroaderterms = 0;
    $countfornarrowerterms = 0; //used to calculate "Top terms"/"Orphan terms"
    foreach ($relationships as $key => $pidarray) {
      foreach ($pidarray as $foreignpid) {
        //update the statistics
        if (in_array($key, $hierarchicalbt)) {
          $context['results']['hierrels']++;
          $countforbroaderterms++;
        }
        if (in_array($key, $hierarchicalnt)) {
          $context['results']['hierrels']++;
          $countfornarrowerterms++;
        }
        if (in_array($key, $equivalence)) {
          $context['results']['equivrels']++;
        }
        if (in_array($key, $associative)) {
          $context['results']['assocrels']++;
        }
        if (in_array($key, $geographic)) {
          $context['results']['georels']++;
        }
        //TODO: check for pidarray being null?
        foreach ($rel_inverse as $forward => $reverse) {
          if (strcmp($key, $forward) == 0) {
            if (!$foreignpid) {
              $errortext = $memberpid . ', ' . $authvar . ' ' . $forward . ' relation has missing pid' . "\n";
              fwrite($invalidtextfile, $errortext);
            }
            else {
              //as $memberpid -> relation -> $foreignpid exists,
              //check that $foreignpid -> inverserelation -> $memberpid exists
              // and report if it doesn't.
              $foreignitem = new Fedora_Item($foreignpid);
              $dc = new dublin_core($foreignitem);
              $titles = $dc->dc['dc:title'];
              $title = $titles[0];

              if (!_relationship_exists($foreignpid, $reverse, $memberpid)) {
                //report the failure here
                $errortext = $foreignpid . ', ' . $title . ' ' . $reverse . ' relation to ' . $memberpid . ' ' . $authvar . ' missing' . "\n";
                fwrite($invalidtextfile, $errortext);
              }
              $pidnamespace = substr($memberpid, 0, strpos($memberpid, ':'));

              if (strcmp($pidnamespace, 'geo') == 0) {
                //need to check that all relationships apart from BroaderThan and Related point to a geo term
                $foreignpidnamespace = substr($foreignpid, 0, strpos($foreignpid, ':'));
                if (strcmp($foreignpidnamespace, 'geo') != 0) {
                  $arraytocheck = array_merge($associative, $hierarchicalbt); //Error not applicable for BT/RT
                  if (!in_array($forward, $arraytocheck)) {
                    $errortext = $memberpid . ', ' . $authvar . ' has relation ' . $forward . ' to ' . $foreignpid . ' ' . $title . ' which is not a GEO term' . "\n";
                    fwrite($invalidtextfile, $errortext);
                  }
                }
              }
            }
          }
        }
      }
    }
    if ($countforbroaderterms == 0) {
      if ($countfornarrowerterms == 0) {
        //can only be an orphan if it's a preferred term
        if (!is_null($madsauthority)) {
          //it's an orphan term
          $context['results']['orphanterms']++;
          $context['results']['orphantermsarray'][] = $authvar;
        }
      }
      else {
        //it's a top term
        $context['results']['topterms']++;
        $context['results']['toptermsarray'][] = $authvar;
      }
    }
    else {
      //it's a normal term, no stats required
    }
  }
  else {
    //failure to create the output files
    $current_set = &_batch_current_set();
    $current_set['success'] = FALSE;
    $context['results'][] = 'Cannot open output file.';
    $context['finished'] = 1.0;
    $returnvalue = FALSE;
  }
  fclose($invalidtextfile);

  return $returnvalue;
}

/**
 * #
 * @param type $success
 * @param type $results
 * @param type $operations
 */
function _tes_validator_batch_finished($success, $results, $operations) {
  if ($success) {
    $message = t('Post validation finished.');
    $reporttextfile = fopen('/tmp/multitesreport.txt', "w+");

    $pt = $results['preferredterms'];
    $npt = $results['nonpreferredterms'];
    $totalterms = intval($pt) + intval($npt);
    $tt = $results['topterms'];
    $ot = $results['orphanterms'];
    $hier = $results['hierrels'];
    $numhier = intval($hier) / 2;
    $equiv = $results['equivrels'];
    $numequiv = intval($equiv) / 2;
    $assoc = $results['assocrels'];
    $georels = $results['georels'];
    $numassocgeo = intval($assoc) + intval($georels);
    $geo = $results['geoterms'];

    $latestupd = $results['latestupd'];
    fwrite($reporttextfile, "STATISTICS:\n");
    fwrite($reporttextfile, "Statistics last updated: " . $latestupd->format('Y-m-d H:i:s') . "\n");
    fwrite($reporttextfile, "Total number of terms: " . $totalterms . "\n");
    fwrite($reporttextfile, "Preferred terms: " . $pt . "\n");
    fwrite($reporttextfile, "Non Preferred terms: " . $npt . "\n");

    fwrite($reporttextfile, "Top terms: " . $tt . "\n");
    foreach ($results['toptermsarray'] as $topterm) {
      fwrite($reporttextfile, "Top term: " . $topterm . "\n");
    }

    fwrite($reporttextfile, "Orphan terms: " . $ot . "\n");
    foreach ($results['orphantermsarray'] as $orphanterm) {
      fwrite($reporttextfile, "Orphan term: " . $orphanterm . "\n");
    }

    fwrite($reporttextfile, "Hierarchical relationships: " . $numhier . "\n");
    fwrite($reporttextfile, "Equivalence relationships: " . $numequiv . "\n");
    fwrite($reporttextfile, "Associative relationships: " . $numassocgeo . "\n");
    fwrite($reporttextfile, "Number of GEO terms: " . $geo . "\n");

    fclose($reporttextfile);

    $postvalfile = '/tmp/multitespostvalidate.csv';
    $sortedpostvalfile = '/tmp/sortedmultitespostvalidate.csv';
    $lines = file($postvalfile);
    natsort($lines);
    $arraywithheadings = 'PID, Message' . "\n" . implode("", $lines);
    file_put_contents($sortedpostvalfile, $arraywithheadings);

    //TODO remove all the unwanted xml files from tmp and return the report files?
    //clear down the pid/tnr/title table
    //$queryresult = db_query('DELETE FROM {%s}', TEMP_PID_TNR_AUTHVAR_TABLE);
  }
  else {
    $message = t('Post validation finished with error - ' . $results[0]);
  }
  drupal_set_message($message);
}

/**
 *
 * @param type $frompid
 * @param type $rdftocheck
 * @param type $topid
 * @return boolean
 */
function _relationship_exists($frompid, $rdftocheck, $topid) {
  $relationfound = FALSE;

  //Get object $frompid, see if it has a RELS-EXT of $rdftocheck -> $topid
  $item = new fedora_item($frompid);
  $relationships = array();
  $relsext = $item->get_datastream_dissemination('RELS-EXT');

  // Parse the RELS-EXT into an associative array.
  $relsextxml = new DOMDocument();
  $relsextxml->loadXML($relsext);
  $relsextxml->normalizeDocument();

  $allTags = array();
  $allTags[] = $relsextxml->getElementsByTagNameNS('*', '*');

  foreach ($allTags as $tags) {
    foreach ($tags as $child) {
      $tagname = $child->tagName;
      $attns = $child->getAttributeNS('http://www.w3.org/1999/02/22-rdf-syntax-ns#', 'resource');
      if ($attns != NULL) {
        $value = preg_replace('/info:fedora\//', '', $attns);

        $relationships[$tagname][] = $value;
        if (strcmp($tagname, $rdftocheck) == 0) {
          //tags match, what about values
          if (strcmp($value, $topid) == 0) {
            //matching rels-ext found
            $relationfound = TRUE;
            break;
          }
          else {
            //tags match, but values don't
          }
        }
        else {
          //tags don't match, not worth checking values
        }
      }
    }
  }
  return $relationfound;
}
