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
 * Implements hook_submit.
 * Return status message of autocomplete path and title strings.
 *
 * @param array $form
 *   standard form variable for a hook
 * @param array $form_state
 *   standard form state variable for a hook
 */
function fba_solution_pack_import_multites_form_relations_submit($form, &$form_state) {

// Need to check if this has come from "ingest" or "relations"
  module_load_include('inc', 'custom_search', 'includes/apachesolr_search');
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'api/fedora_collection');

  $bt_relationship = 'hasBroaderAuthority';
  $nt_relationship = 'hasNarrowerAuthority';
  $rt_relationship = 'hasRelatedAuthority';
  $usefor_relationship = 'useFor';
  $use_relationship = 'useInstead';

  $path = drupal_get_path('module', 'fba_solution_pack_import_multites');


  try {
    //Phase 2 of import/relations/validation
    // All objects have been ingested/updated. 
    // Next - check/add/update the rdf relations
    //Loop through all vocab:* (except collection) objects
    $member_pids = get_related_items_as_array('vocab:collection', 'isMemberOfCollection', 1000000);
    //limit is set to 1 million as there's no "all" value for this function
    foreach ($member_pids as $pid) {
    // get the mads datastream for this object
      $item = new fedora_item($pid);

      $mads = $item->get_datastream_dissemination('MADS');
      $madsxml = new DOMDocument();
      $madsxml->loadXML($mads);
      $madsxml->normalizeDocument();
      $elements = $madsxml->getElementsByTagName('mads');
      foreach ($elements as $node) {
        $relatedelements = $node->getElementsByTagName('related');
        foreach ($relatedelements as $relatedelement) {
          if ($relatedelement->hasAttribute('type')) {
            //could be bt/nt/uf/use
            $typeattribute = $relatedelement->getAttribute('type');
            switch (trim($typeattribute)) {
              case 'broader':
                $btauthority = trim($relatedelement->nodeValue);
                _update_relation_madsrdf($pid, $bt_relationship, $btauthority, 1); //there can only be 1 "broader term"
                break;
              case 'narrower':
                $ntauthority = trim($relatedelement->nodeValue);
                _update_relation_madsrdf($pid, $nt_relationship, $ntauthority);
                break;
              case 'other':
                if ($relatedelement->hasAttribute('otherType')) {
                  $othertypeattribute = $relatedelement->getAttribute('otherType');
                  switch ($othertypeattribute) {
                    case 'used for':
                      $ufauthority = trim($relatedelement->nodeValue);
                      _update_relation_madsrdf($pid, $usefor_relationship, $ufauthority);
                      break;
                    case 'use':
                      $useauthority = trim($relatedelement->nodeValue);
                      _update_relation_madsrdf($pid, $use_relationship, $useauthority, 1); //there can only be 1 "use" term
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
            $rtauthority = trim($relatedelement->nodeValue);
            _update_relation_madsrdf($pid, $rt_relationship, $rtauthority);
          }
        }
      }
    }
    drupal_set_message(t('Processing complete'));
  } catch (Exception $e) {
    form_set_error(NULL, $e->getMessage());
  }
}

/*
 * add $relationship to $pid's relsext (use $madsrelation), foreignpid is in $responsedocs
 * $maxnums is to ensure correct number of relations is true, 0 means any amount is allowed
 * eg broader terms can only have one entry.
 * 
 */
function _update_relation_madsrdf($pid, $relationship, $authority, $maxnum = 0) {

  $item = new fedora_item($pid);
  $itemrels = _fba_get_relationships($item, $relationship);

//ensure relationships don't already exist
  if (empty($itemrels)) {
    //they don't, lets add them

    //find foreignpid from $authority via solr
    $solrsearchterm = 'mads.authority:"' . trim($authority) . '" OR mads.variant:"' . trim($authority) . '"';
    $solrresult = _perform_solr_search($solrsearchterm);
    $total = (int) $solrresult->response->numFound;

    if (($maxnum == 0) || (($maxnum != 0) && ($total == $maxnum))) {
      $rawresponse = $solrresult->getRawResponse();
      $resarray = json_decode($rawresponse, TRUE);
      $resdocs = $resarray['response']['docs'];
      $foreignpid = $resdocs[0][PID];

      //now add relsext link between $pid and $foreignpid of $relationship
      $item->add_relationship($relationship, $foreignpid, 'http://www.loc.gov/mads/rdf/v1#');
    }
    else {
      //incorrect number of items returned from solr search
      $error = '_update_relation_madsrdf: incorrect amount of ' . $relationship . ' for ' . $pid . ' from solr search';
      watchdog('content', $error);
    }
  }
  else {
    //need to enumerate existing relations to see if this one already exists
    $pidrelationshipmatch = FALSE;
    $relationshipmatch = FALSE;

    $solrsearchterm = 'mads.authority:"' . trim($authority) . '" OR mads.variant:"' . ($authority) . '"';
    $solrresult = _perform_solr_search($solrsearchterm);
    $total = (int) $solrresult->response->numFound;
    if (($maxnum == 0) || (($maxnum != 0) && ($total == $maxnum))) {
      $rawresponse = $solrresult->getRawResponse();
      $resarray = json_decode($rawresponse, TRUE);
      $resdocs = $resarray['response']['docs'];
      $foreignpid = $resdocs[0][PID];

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
    else {
      //incorrect number of items returned from solr search
      $error = '_update_relation_madsrdf: incorrect amount of ' . $relationship . ' for ' . $pid . ' from solr search';
      watchdog('content', $error);
    }
  }
}