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

  //TODO: check for fedora being offline here?
  $form = array();
  // Set the form encoding type.
  $form['#attributes']['enctype'] = "multipart/form-data";

  // Add a submit button.
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Process valid multites entries'),
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
function fba_solution_pack_import_multites_form_ingest_submit($form, &$form_state) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');

  if (!$form_state['form_has_errors']) {
    $batch = array(
      'title' => t('Import/update/delete Multites vocab objects'),
      'operations' => array(), // Operations to complete, in order. Defined below.
      'finished' => 'multites_importer_batch_finished', // Last function to call.
      'init_message' => t('Initialising...'),
      'progress_message' => t('Processed @current out of @total.'),
      'error_message' => t('Multites vocab ingest encountered an error.'),
    );

    //clear out the files from previous batch run
    $invalidtextfile = fopen('/tmp/multitesingestfails.csv', "w+");
    fclose($invalidtextfile);


    //need to lookup pid and tnr, in an array in this case
    //$allpids = _fba_get_pids_from_collections();    //all the current pids in fedora commons
    //foreach ($allpids as $vocabpid) {
    //create batch operation for each
    //$batch['operations'][] = array('_tes_importer_batch_preprocess', array($vocabpid));
    //}

    $fedoratnrs = array_unique(file('/tmp/multitesfedoratnr.txt', FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES));


    //now all the tnr's from the latest multites export
    $listofmultitestnrs = file('/tmp/FBAtnr.txt', FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);

    sort($fedoratnrs);
    sort($listofmultitestnrs);

    $tnrdiff = array_diff($fedoratnrs, $listofmultitestnrs);    //list of tnr's that have been deleted from multites

    if (count($tnrdiff) > 0) {
      foreach ($tnrdiff as $index => $deletedtnr) {
        //get the pid from the temporary database table
        $valresult = db_result(db_query('SELECT pid FROM {%s} WHERE tnr = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $deletedtnr));

        $batch['operations'][] = array('_tes_importer_batch_delete', array($deletedtnr, $valresult));
      }
    }
    //the rest of the list can be added/updated
    foreach ($listofmultitestnrs as $tnr) {
      $tnrxmlfilename = '/tmp/FBAtes' . trim($tnr) . '.xml';
      $inttnr = intval($tnr);
      //get the pid from the temporary database table
      $foundpid = db_result(db_query('SELECT pid FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $inttnr));
      //if there's no found pid, then the batch job will add rather than update
      $batch['operations'][] = array('_tes_importer_batch_add_update', array($tnrxmlfilename, $foundpid));
    }
    // Tip the first domino.
    _tes_import_batch_process_pages($batch);
  }
}

/*
 * This function is necessary as the Drupal batch api runs out of memory when being called
 * from a large form submit handler
 */

function _tes_import_batch_process_pages($batch) {
  batch_set($batch);
  batch_process();
}

/**
 * @param array $form
 * @param array $form_state
 */
function fba_solution_pack_import_multites_form_ingest_validate($form, &$form_state) {
  $tnrlistfilename = '/tmp/FBAtnr.txt';
  $form_state['form_has_errors'] = FALSE;

  if (!file_exists($tnrlistfilename)) {
    $form_state['form_has_errors'] = TRUE;
    form_set_error('', t($tnrlistfilename . ' cannot be opened.'));
  }
}

function _tes_importer_batch_preprocess($pid, &$context) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');

  $fedoraobject = new Fedora_Item($pid);

  $mads = $fedoraobject->get_datastream_dissemination('MADS');

  if (!$mads) {
    $invalidtextfile = fopen('/tmp/multitesingestfails.csv', "a+");
    fwrite($invalidtextfile, 'Object with pid - ' . $pid . ' has no MADS datastream');

    fclose($invalidtextfile);
  }
  else {
    $simplemads = simplexml_load_string($mads);

    $madsidattrs = $simplemads->identifier->attributes();
    $madsidtype = (string) $madsidattrs['type'];

    if (strcmp($madsidtype, 'TNR') == 0) {
      $madsidentifierval = (string) $simplemads->identifier;
      //$tnrvspid[$madsidentifierval] = $vocabpid;
      $context['results']['tnrvspid'][$madsidentifierval] = $pid;
      //$alltnrs[] = $madsidentifierval;
      $context['results']['alltnrs'][] = $madsidentifierval;
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
 * @param type $tnr
 * @param type $pid
 * @param type $context
 * @return boolean
 */
function _tes_importer_batch_delete($tnr, $pid, &$context) {
  //$pid needs to be deleted unless it has rdf's that aren't vocab/geo based
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $returnvalue = TRUE;
  $fedoraobject = new Fedora_Item($pid);
  $relationships = _fba_get_relationships($fedoraobject);
  $numrelationships = count($relationships);
  for ($i = 0; $i < $numrelationships; $i++) {
    if ((strpos($relationships[$i]['predicate'], 'rdf:') !== FALSE)) {
      unset($relationships[$i]);
    }
    if ((strpos($relationships[$i]['predicate'], 'isMemberOfCollection') !== FALSE)) {
      unset($relationships[$i]);
    }
    if ((strpos($relationships[$i]['predicate'], 'hasModel') !== FALSE)) {
      unset($relationships[$i]);
    }
    if ((strpos($relationships[$i]['object'], 'vocab:') !== FALSE)) {
      unset($relationships[$i]);
    }
    if ((strpos($relationships[$i]['object'], 'geo:') !== FALSE)) {
      unset($relationships[$i]);
    }
  }

  //See if any are left
  $numrelationshipsleft = count($relationships);

  if ($numrelationshipsleft > 0) {
    //can't delete this object yet
    $invalidtextfile = fopen('/tmp/multitesingestfails.csv', "a+");
    if ($invalidtextfile) {
      fwrite($invalidtextfile, 'Object with pid - ' . $pid . ', tnr - ' . $tnr . ' cannot be deleted, still has non vocab rdf links');
    }
    else {
      //failure to create the output files
      $current_set = &_batch_current_set();
      $current_set['success'] = FALSE;
      $context['results'][] = 'batch_delete: Cannot write to error file.';
      $context['finished'] = 1.0;
      $returnvalue = FALSE;
    }
    $fedoraobject->forget();
  }
  else {
    //ok to get rid of it.
    $fedoraobject->purge('Object deleted by multiTES batch ingest');
    //There doesn't seem to be any error checking usually if the purge has worked or not!
  }
  fclose($invalidtextfile);
  return $returnvalue;
}

/**
 *
 * @global type $user
 * @global type $user
 * @param type $xmlfilename
 * @param type $pid
 * @param type $context
 * @return boolean
 */
function _tes_importer_batch_add_update($xmlfilename, $pid, &$context) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'api/dublin_core');
  if (file_exists($xmlfilename)) {
    $filesimplexml = simplexml_load_file($xmlfilename);

    $ispreferredterm = FALSE;
    if (isset($filesimplexml->DESCRIPTOR)) {
      $ispreferredterm = TRUE;
    }
    else {
      $ispreferredterm = FALSE;
    }

    if ($filesimplexml) {
      // Need to change date formats
      $inpdate = (string) $filesimplexml->INP;
      $appdate = (string) $filesimplexml->APP;
      $upddate = (string) $filesimplexml->UPD;

      $inpdatesolrformat = _format_solr_date($inpdate);
      $appdatesolrformat = _format_solr_date($appdate);
      $upddatesolrformat = _format_solr_date($upddate);
      //check the above for FALSE if date formatting has failed?

      $filesimplexml->INP = $inpdatesolrformat;
      $filesimplexml->APP = $appdatesolrformat;
      $filesimplexml->UPD = $upddatesolrformat;

      $filexml = $filesimplexml->asXML();

      if ($pid) {
        //Object already exists, do an update if necessary
        //Check if any fields have altered
        //pass the concept through the tes to mads stylesheet
        $xml = new DOMDocument();
        $xml->loadXML($filexml);

        $xsl = new DOMDocument();
        $path = drupal_get_path('module', 'fba_solution_pack_import_multites');
        $xsl->load($path . '/xslt/convertTEStoMADS.xsl');

        $proc = new XSLTProcessor();
        $proc->importStylesheet($xsl);
        $doc = $proc->transformToDoc($xml);

        $madselements = $doc->getElementsByTagName('mads');

        //need to exclude the outer tag to ensure comparison can work
        $innerdoc = new DOMDocument;
        while ($node = $madselements->item($i)) {
          $domNode = $innerdoc->importNode($node, true);
          $innerdoc->appendChild($domNode);
          $i++;
        }
        $innerdocasxml = $innerdoc->saveXML();
        $simplefilemads = simplexml_load_string($innerdocasxml);
        /* code to add pid to vocab terms
          foreach ($simplefilemads as $subfilekey => $subfilemads) {
          if (($subfilekey != 'authority') && ($subfilekey != 'variant')) {
          if ($subfilemads->topic) {
          $currentval = (string) $subfilemads->topic;
          //get the pid from the temporary database table
          $valresult = db_result(db_query('SELECT pid FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $currentval));

          $amendedval = $currentval . ' ||' . $valresult . '||';

          $subfilemads->topic = $amendedval;
          }
          }
          }

          $simplefilemadsasxml = $simplefilemads->asXML();
         */
        //get fedora object into same format
        $currentobject = new fedora_item($pid);

        $mads = $currentobject->get_datastream_dissemination('MADS');
        $simplemads = simplexml_load_string($mads);

        //do the comparison
        $domadsmatch = xml_is_equal($simplefilemads, $simplemads);

        //Check the DC
        //DTC111 SFOX 20131002 DC title wasn't being updated,
        //check if this needs to happen
        $dc = new dublin_core($currentobject);
        $fedoratitles = $dc->dc['dc:title'];
        $fedoratitle = $fedoratitles[0];

        if ($ispreferredterm) {
          //handle preferred term
          $titletocompare = (string) $simplefilemads->authority->topic;
        }
        else {
          //handle non preferred term
          $titletocompare = (string) $simplefilemads->variant->topic;
        }

        //Update the DC datastream title
        if (strcmp($titletocompare, $fedoratitle) != 0) {
          //value has changed, update required
          $dc->set_element('dc:title', array($titletocompare));
          $dc->save();
          global $user;
          $currentobject->modify_object($titletocompare, 'A', $user->uid, 'Vocab-Geo terms batch ingest');    // Set the label
        }
        //Now update MADS if required (a string is returned if the MADS don't match)
        if (!is_bool($domadsmatch)) {

          // $simplefilemads needs ||PID|| appending onto all relations

          $madsupdateretval = $currentobject->modify_datastream_by_value($innerdocasxml, 'MADS', 'MADS Record', 'text/xml', FALSE, 'Vocab-Geo terms batch ingest');
          if ($madsupdateretval !== NULL) {
            //update worked
          }
          else {
            $context['results'][] = 'batch ingest file ' . $xmlfilename . ': MADS update failed.';
            break;
          }
        }
        _tes_db_populate($pid, (string) $filesimplexml->TNR, $titletocompare);
      }
      else {
        //New object to be created
        //pass the concept through the tes to mads stylesheet
        $xml = new DOMDocument();
        $xml->loadXML($filexml);

        $xsl = new DOMDocument();
        $path = drupal_get_path('module', 'fba_solution_pack_import_multites');
        $xsl->load($path . '/xslt/convertTEStoMADS.xsl');

        $proc = new XSLTProcessor();
        $proc->importStylesheet($xsl);
        $doc = $proc->transformToDoc($xml);
        $simplefilemads = simplexml_load_string($doc->saveXML());
        /* future code for adding pid to vocab terms
          foreach ($simplefilemads as $subfilekey => $subfilemads) {
          if (($subfilekey != 'authority') && ($subfilekey != 'variant')) {
          if ($subfilemads->topic) {
          $currentval = (string) $subfilemads->topic;
          //get the pid from the temporary database table
          $valresult = db_result(db_query('SELECT pid FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $currentval));

          if (!$valresult) {
          $amendedval = $currentval . ' ||' . $valresult . '||';
          $subfilemads->topic = $amendedval;
          //will need a validation step to show that there isn't a pid on this
          }
          }
          }
          }
         */
        $simplefilemadsasxml = $simplefilemads->asXML();

        //
        //
        // add this object to fedora via the vocab/geo content model
        $sctag = (string) $filesimplexml->SC;
        if (strcmp($sctag, 'GEO') == 0) {
          $namespace = 'geo';
        }
        else {
          $namespace = 'vocab';
        }
        $collection_pid = 'info:fedora/' . $namespace . ':collection';
        $nextavailablepid = fedora_item::get_next_PID_in_namespace($namespace);
        global $user;
        if ($ispreferredterm) {
          //handle preferred term
          $title = (string) $simplefilemads->authority->topic;
          $item = fedora_item::ingest_new_item($nextavailablepid, 'A', $title, $user->name);
        }
        else {
          //handle non preferred term
          $title = (string) $simplefilemads->variant->topic;
          $item = fedora_item::ingest_new_item($nextavailablepid, 'A', $title, $user->name);
        }
        $item->add_relationship('isMemberOfCollection', $collection_pid, RELS_EXT_URI);
        $item->add_relationship('hasModel', 'fba:' . $namespace . '_cmodel', FEDORA_MODEL_URI);
        $thumbnail = $path . '/images/vocabicon.png';
        $item->add_datastream_from_file($thumbnail, 'TN');

        $return_value = $item->add_datastream_from_string($simplefilemadsasxml, 'MADS', 'MADS Record', 'text/xml', 'X');

        //update the temporary lookup database table
        _tes_db_populate($nextavailablepid, (string) $filesimplexml->TNR, $title);
      }
    }
    else {
      //$xml is null, report an error?
      $current_set = &_batch_current_set();
      $current_set['success'] = FALSE;
      $context['results'][] = 'batch ingest file ' . $xmlfilename . ': XML is empty.';
      $context['finished'] = 1.0;
      $returnvalue = FALSE;

      break;
    }
  }
  else {
    $current_set = &_batch_current_set();
    $current_set['success'] = FALSE;
    $context['results'][] = 'batch ingest: error processing file ' . $xmlfilename;
    $context['finished'] = 1.0;
    $returnvalue = FALSE;
  }
  return TRUE;
}

/**
 *
 * @param type $success
 * @param type $results
 * @param type $operations
 */
function multites_importer_batch_finished($success, $results, $operations) {
  if ($success) {
    $message = t('All Multites records processed.');
  }
  else {
    $message = t('Finished with error - ' . $results[0]);
  }

  drupal_set_message($message);
}

/**
 *
 * @param type $dateastext
 * @return string
 */
function _format_solr_date($dateastext) {
  $UTCdateformatstring = 'Y-m-d\TH:i:s';    // TODO: need to append '.\000Z' somehow;
  $datetimevalue = new DateTime($dateastext);
  //Translate to YYYY-MM-DDTHH:MM:SS.uuuZ
  $solrformat = $datetimevalue->format($UTCdateformatstring) . '.000Z';
  return $solrformat;
}

/**
 * from http://www.jevon.org/wiki/Comparing_Two_SimpleXML_Documents
 * @param SimpleXMLElement $xml1
 * @param SimpleXMLElement $xml2
 * @param type $text_strict
 * @return string|boolean
 */
function xml_is_equal(SimpleXMLElement $xml1, SimpleXMLElement $xml2, $text_strict = false) {
  //
  // compare text content
  if ($text_strict) {
    if ("$xml1" != "$xml2")
      return "mismatched text content (strict)";
  } else {
    if (trim("$xml1") != trim("$xml2"))
      return "mismatched text content";
  }

  // check all attributes
  $search1 = array();
  $search2 = array();
  foreach ($xml1->attributes() as $a => $b) {
    $search1[$a] = "$b";     // force string conversion
  }
  foreach ($xml2->attributes() as $a => $b) {
    $search2[$a] = "$b";
  }
  if ($search1 != $search2)
    return "mismatched attributes";

  // check all namespaces
  $ns1 = array();
  $ns2 = array();
  foreach ($xml1->getNamespaces() as $a => $b) {
    $ns1[$a] = "$b";
  }
  foreach ($xml2->getNamespaces() as $a => $b) {
    $ns2[$a] = "$b";
  }
  if ($ns1 != $ns2)
    return "mismatched namespaces";

  // get all namespace attributes
  foreach ($ns1 as $ns) {      // don't need to cycle over ns2, since its identical to ns1
    $search1 = array();
    $search2 = array();
    foreach ($xml1->attributes($ns) as $a => $b) {
      $search1[$a] = "$b";
    }
    foreach ($xml2->attributes($ns) as $a => $b) {
      $search2[$a] = "$b";
    }
    if ($search1 != $search2)
      return "mismatched ns:$ns attributes";
  }

  // get all children
  $search1 = array();
  $search2 = array();
  foreach ($xml1->children() as $b) {
    if (!isset($search1[$b->getName()]))
      $search1[$b->getName()] = array();
    $search1[$b->getName()][] = $b;
  }
  foreach ($xml2->children() as $b) {
    if (!isset($search2[$b->getName()]))
      $search2[$b->getName()] = array();
    $search2[$b->getName()][] = $b;
  }
  // cycle over children
  if (count($search1) != count($search2))
    return "mismatched children count";     // xml2 has less or more children names (we don't have to search through xml2's children too)
  foreach ($search1 as $child_name => $children) {
    if (!isset($search2[$child_name]))
      return "xml2 does not have child $child_name";     // xml2 has none of this child
    if (count($search1[$child_name]) != count($search2[$child_name]))
      return "mismatched $child_name children count";     // xml2 has less or more children
    foreach ($children as $child) {
      // do any of search2 children match?
      $found_match = false;
      $reasons = array();
      foreach ($search2[$child_name] as $id => $second_child) {
        if (($r = xml_is_equal($child, $second_child)) === true) {
          // found a match: delete second
          $found_match = true;
          unset($search2[$child_name][$id]);
        }
        else {
          $reasons[] = $r;
        }
      }
      if (!$found_match)
        return "xml2 does not have specific $child_name child: " . implode("; ", $reasons);
    }
  }

  // finally, cycle over namespaced children
  foreach ($ns1 as $ns) {      // don't need to cycle over ns2, since its identical to ns1
    // get all children
    $search1 = array();
    $search2 = array();
    foreach ($xml1->children() as $b) {
      if (!isset($search1[$b->getName()]))
        $search1[$b->getName()] = array();
      $search1[$b->getName()][] = $b;
    }
    foreach ($xml2->children() as $b) {
      if (!isset($search2[$b->getName()]))
        $search2[$b->getName()] = array();
      $search2[$b->getName()][] = $b;
    }
    // cycle over children
    if (count($search1) != count($search2))
      return "mismatched ns:$ns children count";     // xml2 has less or more children names (we don't have to search through xml2's children too)
    foreach ($search1 as $child_name => $children) {
      if (!isset($search2[$child_name]))
        return "xml2 does not have ns:$ns child $child_name";     // xml2 has none of this child
      if (count($search1[$child_name]) != count($search2[$child_name]))
        return "mismatched ns:$ns $child_name children count";     // xml2 has less or more children
      foreach ($children as $child) {
        // do any of search2 children match?
        $found_match = false;
        foreach ($search2[$child_name] as $id => $second_child) {
          if (xml_is_equal($child, $second_child) === true) {
            // found a match: delete second
            $found_match = true;
            unset($search2[$child_name][$id]);
          }
        }
        if (!$found_match)
          return "xml2 does not have specific ns:$ns $child_name child";
      }
    }
  }

  // if we've got through all of THIS, then we can say that xml1 has the same attributes and children as xml2.
  return true;
}

/**
 *
 * @param type $pid
 * @param type $tnr
 * @param type $authvar
 * @return boolean
 */
function _tes_db_populate($pid, $tnr, $authvar) {
  $queryresult = db_query('SELECT count(authvar) FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $authvar);
  $numberofterms = db_result($queryresult);
  if ($numberofterms == 0) {
    $object = new stdClass();
    $object->pid = $pid;
    $object->tnr = $tnr;
    $object->authvar = $authvar;
    return drupal_write_record(TEMP_PID_TNR_AUTHVAR_TABLE, $object) !== FALSE;
  }

  if ($numberofterms == 1) {
    $queryresult = db_result(db_query('SELECT pid,tnr FROM {%s} WHERE authvar = "%s"', TEMP_PID_TNR_AUTHVAR_TABLE, $authvar));
    if (strcmp($queryresult, $pid) != 0) {
      //don't need to update the record if pid and authvar match
      $object = new stdClass();
      $object->pid = $pid;
      $object->tnr = $tnr;
      return drupal_write_record(TEMP_PID_TNR_AUTHVAR_TABLE, $object, 'authvar') !== FALSE;
    }
  }
  return FALSE;
}