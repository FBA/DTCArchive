<?php

/**
 * Implements hook_form().
 *
 * Landing page for import function
 *
 * @return string
 *   html formatted output
 */
function fba_solution_pack_import_multites_form_relations(&$form_state = array()) {

  $form = array();
  // Set the form encoding type.
  $form['#attributes']['enctype'] = "multipart/form-data";

// Add a submit button.
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Process rdfs for all vocab objects'),
  );

  return $form;
}

/** RDF Relations section * */

/**
 *
 * @param type $form
 * @param type $form_state
 */
function fba_solution_pack_import_multites_form_relations_submit($form, &$form_state) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_collection');

  if (!$form_state['form_has_errors']) {
    $batch = array(
      'title' => t('Process RDF Relations for Multites vocab/geo objects'),
      'operations' => array(), // Operations to complete, in order. Defined below.
      'finished' => 'multites_relations_batch_finished', // Last function to call.
      'init_message' => t('Initialising...'),
      'progress_message' => t('Processed @current out of @total.'),
      'error_message' => t('Multites vocab relations check encountered an error.'),
    );
    $member_pids_vocab = get_related_items_as_array('vocab:collection', 'isMemberOfCollection', 1000000);
    $member_pids_geo = get_related_items_as_array('geo:collection', 'isMemberOfCollection', 1000000);
    //limit is set to 1 million as there's no "all" value for this function
    foreach ($member_pids_vocab as $pid) {
      $batch['operations'][] = array('_tes_relations_batch_first', array($pid));
    }
    foreach ($member_pids_geo as $pid) {
      $batch['operations'][] = array('_tes_relations_batch_first', array($pid));
    }
    // Tip the first domino.
    _tes_import_batch_process_pages($batch);
  }
}

/**
 *
 * @param type $pid
 * @param type $context
 * @return boolean
 */
function _tes_relations_batch_first($pid, &$context) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');

  $bt_relationship = 'hasBroaderAuthority';
  $nt_relationship = 'hasNarrowerAuthority';
  $rt_relationship = 'hasRelatedAuthority';
  $gf_relationship = 'hasGeographicFeature';
  $igf_relationship = 'isGeographicFeatureOf';
  $pu_relationship = 'hasPreferredUnit';
  $usefor_relationship = 'useFor';
  $use_relationship = 'useInstead';

  $item = new fedora_item($pid);

  $relsextchanges = array();
  $relsext = _fba_get_relationships($item);
  foreach ($relsext as $objectrelation) {
    switch ($objectrelation['predicate']) {
      case $bt_relationship:
      case $nt_relationship:
      case $rt_relationship:
      case $gf_relationship:
      case $igf_relationship:
      case $pu_relationship:
      case $usefor_relationship:
      case $use_relationship: {
          $relsextchanges[] = array($objectrelation['subject'], $objectrelation['predicate'], $objectrelation['object']);
          break;
        }
    }
  }

  $mads = $item->get_datastream_dissemination('MADS');
  $madschanges = array();

  $madsxml = new DOMDocument();
  $madsxml->loadXML($mads);
  $madsxml->normalizeDocument();
  $elements = $madsxml->getElementsByTagName('mads');
  foreach ($elements as $node) {
    $relatedelements = $node->getElementsByTagName('related');
    foreach ($relatedelements as $relatedelement) {
      if ($relatedelement->hasAttribute('type')) {
        //could be bt/nt/uf/use/gf/igf/pu
        $typeattribute = $relatedelement->getAttribute('type');
        switch (trim($typeattribute)) {
          case 'broader':
            $authority = trim($relatedelement->nodeValue);
            $authority = str_replace('%', '\%', $authority);
            $foreignpid = db_result(db_query('SELECT pid FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $authority));
            if ($foreignpid !== FALSE) {
              $madschanges[] = array($pid, $bt_relationship, $foreignpid);
            }
            else {
//              $madschanges[] = array($pid, $usefor_relationship, ''); //put it as blank so the validator will pick it up later
            }
            break;
          case 'narrower':
            $authority = trim($relatedelement->nodeValue);
            $authority = str_replace('%', '\%', $authority);
            $foreignpid = db_result(db_query('SELECT pid FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $authority));
            if ($foreignpid !== FALSE) {
              $madschanges[] = array($pid, $nt_relationship, $foreignpid);
            }
            else {
//              $madschanges[] = array($pid, $usefor_relationship, ''); //put it as blank so the validator will pick it up later
            }
            break;
          case 'other':
            if ($relatedelement->hasAttribute('otherType')) {
              $othertypeattribute = $relatedelement->getAttribute('otherType');
              switch ($othertypeattribute) {
                case 'used for':
                  $authority = trim($relatedelement->nodeValue);
                  $authority = str_replace('%', '\%', $authority);
                  $foreignpid = db_result(db_query('SELECT pid FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $authority));
                  if ($foreignpid !== FALSE) {
                    $madschanges[] = array($pid, $usefor_relationship, $foreignpid);
                  }
                  else {
//                    $madschanges[] = array($pid, $usefor_relationship, ''); //put it as blank so the validator will pick it up later
                  }
                  break;
                case 'use':
                  $authority = trim($relatedelement->nodeValue);
                  $authority = str_replace('%', '\%', $authority);
                  $foreignpid = db_result(db_query('SELECT pid FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $authority));
                  if ($foreignpid !== FALSE) {
                    $madschanges[] = array($pid, $use_relationship, $foreignpid);
                  }
                  else {
//                    $madschanges[] = array($pid, $usefor_relationship, ''); //put it as blank so the validator will pick it up later
                  }
                  break;
                case 'is geographic feature of':
                  $authority = trim($relatedelement->nodeValue);
                  $authority = str_replace('%', '\%', $authority);
                  $foreignpid = db_result(db_query('SELECT pid FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $authority));
                  if ($foreignpid !== FALSE) {
                    $madschanges[] = array($pid, $igf_relationship, $foreignpid);
                  }
                  else {
//                    $madschanges[] = array($pid, $usefor_relationship, ''); //put it as blank so the validator will pick it up later
                  }
                  break;
                case 'geographic feature':
                  $authority = trim($relatedelement->nodeValue);
                  $authority = str_replace('%', '\%', $authority);
                  $foreignpid = db_result(db_query('SELECT pid FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $authority));
                  if ($foreignpid !== FALSE) {
                    $madschanges[] = array($pid, $gf_relationship, $foreignpid);
                  }
                  else {
//                    $madschanges[] = array($pid, $usefor_relationship, ''); //put it as blank so the validator will pick it up later
                  }
                  break;
                case 'preferred unit':
                  $authority = trim($relatedelement->nodeValue);
                  $authority = str_replace('%', '\%', $authority);
                  $foreignpid = db_result(db_query('SELECT pid FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $authority));
                  if ($foreignpid !== FALSE) {
                    $madschanges[] = array($pid, $pu_relationship, $foreignpid);
                  }
                  else {
//                    $madschanges[] = array($pid, $usefor_relationship, ''); //put it as blank so the validator will pick it up later
                  }
                  break;
                default:
                  break;
              }
            }
            break;
          default:
            break;
        }
      }
      else {
        //must be rt
        $authority = trim($relatedelement->nodeValue);
        $authority = str_replace('%', '\%', $authority);
        $foreignpid = db_result(db_query('SELECT pid FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $authority));
        if ($foreignpid !== FALSE) {
          $madschanges[] = array($pid, $rt_relationship, $foreignpid);
        }
        else {
//          $madschanges[] = array($pid, $usefor_relationship, ''); //put it as blank so the validator will pick it up later
        }
      }
    }
  }

  //purge_relationship($relationship, $object) for stuff that shouldn't be there
  //add_relationship($relationship, $object, $namespaceURI = RELS_EXT_URI, $literal_value = RELS_TYPE_URI) for a new un
  $itemtwo = array();
  foreach ($madschanges as $key => $row) {
    $madsitemtwo[$key] = $row[2];
  }
  if (is_array($madsitemtwo)) {
    array_multisort($madsitemtwo, SORT_ASC, $madschanges);
  }

  $itemtwo = array();
  foreach ($relsextchanges as $key => $row) {
    $relsitemtwo[$key] = $row[2];
  }
  if (is_array($relsitemtwo)) {
    array_multisort($relsitemtwo, SORT_ASC, $relsextchanges);
  }

  $madsdiff = arr_diff($madschanges, $relsextchanges);
  foreach ($madsdiff as $reltoadd) {
    if ($reltoadd[1]) {
      switch ($reltoadd[1]) {
        case $gf_relationship:
        case $igf_relationship: {
            $item->add_relationship($reltoadd[1], $reltoadd[2], 'http://fba.org.uk/schemas/fba/fba_rdf/1.0.0/');
            break;
          }
        default: {
            $item->add_relationship($reltoadd[1], $reltoadd[2], 'http://www.loc.gov/mads/rdf/v1#');
          }
      }
    }
  }
  $relsdiff = arr_diff($relsextchanges, $madschanges);
  foreach ($relsdiff as $reltoremove) {
    if ($reltoremove[1]) {
      if ($reltoremove[2] == FALSE) {
        $reltoremove[2] = NULL;
      }
      switch ($reltoremove[1]) {
        case $gf_relationship:
        case $igf_relationship: {
            $item->purge_relationships($reltoremove[1], $reltoremove[2], 'http://fba.org.uk/schemas/fba/fba_rdf/1.0.0/', RELS_TYPE_STRING);
            //$item->purge_relationships($reltoremove[1], $reltoremove[2]);
            break;
          }
        default: {
            $item->purge_relationships($reltoremove[1], $reltoremove[2], 'http://www.loc.gov/mads/rdf/v1#', RELS_TYPE_STRING);
            //$item->purge_relationship($reltoremove[1], $reltoremove[2]);
          }
      }
    }
  }
  return TRUE;
}

/**
 * Function code taken from http://uk1.php.net/array_diff
 *
 * @param type $a1
 * @param type $a2
 * @return type
 */
function arr_diff($a1, $a2) {
  foreach ($a1 as $k => $v) {
    unset($dv);
    if (is_int($k)) {
      // Compare values
      if (array_search($v, $a2) === false)
        $dv = $v;
      else if (is_array($v))
        $dv = arr_diff($v, $a2[$k]);
      if ($dv)
        $diff[] = $dv;
    }else {
      // Compare noninteger keys
      if (!$a2[$k])
        $dv = $v;
      else if (is_array($v))
        $dv = arr_diff($v, $a2[$k]);
      if ($dv)
        $diff[$k] = $dv;
    }
  }
  return $diff;
}

/**
 *
 * @param type $success
 * @param type $results
 * @param type $operations
 */
function multites_relations_batch_finished($success, $results, $operations) {
  if ($success) {
    $message = t('Relations processed.');
  }
  else {
    $message = t('Relations finished with error - ' . $results[0]);
  }
  drupal_set_message($message);
}

/*
 * add $relationship to $pid's relsext (use $madsrelation), foreignpid is in $responsedocs
 * $maxnums is to ensure correct number of relations is true, 0 means any amount is allowed
 * eg broader terms can only have one entry.
 *
 */

function _update_relation_madsrdf($pid, $relationship, $authority) {

  $item = new fedora_item($pid);
  $itemrels = _fba_get_relationships($item, $relationship);
  //ensure relationships don't already exist
  if (empty($itemrels)) {
    //they don't, lets add them
    //find foreignpid from $authority
    $foreignpid = db_result(db_query('SELECT pid FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $authority));

    //now add relsext link between $pid and $foreignpid of $relationship
    $item->add_relationship($relationship, $foreignpid, 'http://www.loc.gov/mads/rdf/v1#');
  }
  else {
    //need to enumerate existing relations to see if this one already exists
    $pidrelationshipmatch = FALSE;
    $relationshipmatch = FALSE;
    $foreignpid = db_result(db_query('SELECT pid FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $authority));

    foreach ($itemrels as $itemrel) {
      if (isset($itemrel[predicate]) && $itemrel[predicate] == $relationship) {
        //found hasNarrowerAuthority
        if (isset($itemrel[object]) && $itemrel[object] == $foreignpid) {
          //and it matches on pid as well
          $pidrelationshipmatch = TRUE;
          $relationshipmatch = TRUE;
          break;
        }
        else {
          //pid doesn't
          $relationshipmatch = TRUE;
        }
      }
      else {
        //place holder
      }
    }
    //key value pair might already exist
    if ($pidrelationshipmatch) {
      //pid found in foreign term relsext
    }
    else {
      //terms found, this pid isn't one of them
      $item->add_relationship($relationship, $foreignpid, 'http://www.loc.gov/mads/rdf/v1#');
    }
  }
}

