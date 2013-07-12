<?php

//TODO: what about duplication plus other nasties?

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

  // Need to check if this has come from "ingest" or "validator"
  module_load_include('inc', 'custom_search', 'includes/apachesolr_search');
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'api/fedora_collection');

  $rel_inverse["useInstead"] = "useFor";
  $rel_inverse["useFor"] = "useInstead";
  $rel_inverse["hasBroaderAuthority"] = "hasNarrowerAuthority";
  $rel_inverse["hasNarrowerAuthority"] = "hasBroaderAuthority";
  $rel_inverse["hasRelatedAuthority"] = "hasRelatedAuthority";

  $errorlist = array();
  try {

    $totalErrors = 0;

//Pass Number 2
    // All objects have been ingested/updated. 
    // Next - check/add/update the rdf relations
    //Loop through all vocab:* (except collection) objects
    $member_pids = get_related_items_as_array('vocab:collection', 'isMemberOfCollection', 1000000); //limit is set to 1 million
    foreach ($member_pids as $memberpid) {
      // get the mads datastream for this object
      $item = new fedora_item($memberpid);

      $relationships = array();
      try {
        $relsext = $item->get_datastream_dissemination('RELS-EXT');
      } catch (exception $e) {
        drupal_set_message(t("Error retrieving RELS-EXT of object $memberpid"), 'error');
        return $relationships;
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

      foreach ($relationships as $key => $pidarray) {
        switch ($key) {
          case "hasBroaderAuthority": {
              //only 1 allowed, but using standard loop for now
              foreach ($pidarray as $foreignpid) {
                //as $memberpid -> relation -> $foreignpid exists,
                //check that $foreignpid -> inverserelation -> $memberpid exists
                // and report if it doesn't.
                if (!_relationship_exists($foreignpid, $rel_inverse["hasBroaderAuthority"], $memberpid)) {
                  //report the failure here
                  $errorlist[] = $foreignpid . ' ' . $rel_inverse["hasBroaderAuthority"] . ' ' . $memberpid . ' missing';
                  $totalErrors++;
                }
              }
              break;
            }
          case "hasNarrowerAuthority": {
              foreach ($pidarray as $foreignpid) {
                //as $memberpid -> relation -> $foreignpid exists,
                //check that $foreignpid -> inverserelation -> $memberpid exists
                // and report if it doesn't.
                if (!_relationship_exists($foreignpid, $rel_inverse["hasNarrowerAuthority"], $memberpid)) {
                  //report the failure here
                  $errorlist[] = $foreignpid . ' ' . $rel_inverse["hasNarrowerAuthority"] . ' ' . $memberpid . ' missing';
                  $totalErrors++;
                }
              }
              break;
            }
          case "hasRelatedAuthority": {
              foreach ($pidarray as $foreignpid) {
                //as $memberpid -> relation -> $foreignpid exists,
                //check that $foreignpid -> inverserelation -> $memberpid exists
                // and report if it doesn't.
                if (!_relationship_exists($foreignpid, $rel_inverse["hasRelatedAuthority"], $memberpid)) {
                  //report the failure here
                  $errorlist[] = $foreignpid . ' ' . $rel_inverse["hasRelatedAuthority"] . ' ' . $memberpid . ' missing';
                  $totalErrors++;
                }
              }
              break;
            }
          case "useInstead": {
              foreach ($pidarray as $foreignpid) {
                //as $memberpid -> relation -> $foreignpid exists,
                //check that $foreignpid -> inverserelation -> $memberpid exists
                // and report if it doesn't.
                if (!_relationship_exists($foreignpid, $rel_inverse["useInstead"], $memberpid)) {
                  //report the failure here
                  $errorlist[] = $foreignpid . ' ' . $rel_inverse["useInstead"] . ' ' . $memberpid . ' missing';
                  $totalErrors++;
                }
              }
              break;
            }
          case "useFor": {
              foreach ($pidarray as $foreignpid) {
                //as $memberpid -> relation -> $foreignpid exists,
                //check that $foreignpid -> inverserelation -> $memberpid exists
                // and report if it doesn't.
                if (!_relationship_exists($foreignpid, $rel_inverse["useFor"], $memberpid)) {
                  //report the failure here
                  $errorlist[] = $foreignpid . ' ' . $rel_inverse["useFor"] . ' ' . $memberpid . ' missing';
                  $totalErrors++;
                }
              }
              break;
            }
          default: {
              break;
            }
        }
      }
    }
    echo 'hello world';
    drupal_set_message(t('Processing complete - Number of errors:' . $totalErrors . ';'));
  } catch (Exception $e) {
    form_set_error(NULL, $e->getMessage());
  }
}

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