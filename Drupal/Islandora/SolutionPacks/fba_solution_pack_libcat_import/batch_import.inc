<?php

$keywordarray[] = NULL;
$categoryArray[] = NULL;
$waterAuthArray[] = NULL;

$sInvalidElementsFile;
$fInvalidElements = NULL;

$nTotalsInvalidElements = 0;

function fba_solution_pack_libcat_import_form_batch_import(&$form_state = array()) {
  $form = array();
// Set the form encoding type.
  $form['#attributes'] = array('enctype' => "multipart/form-data");

// Add a submit button.
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Batch import /tmp/FBAvalidobjects.xml file'),
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
function fba_solution_pack_libcat_import_form_batch_import_submit($form, &$form_state) {
  if (!$form_state['form_has_errors']) {
    $batch = array(
      'title' => t('Import/update/delete Library Catalogue objects'),
      'operations' => array(), // Operations to complete, in order. Defined below.
      'finished' => '_importer_batch_finished', // Last function to call.
      'init_message' => t('Initialising...'),
      'progress_message' => t('Processed @current out of @total.'),
      'error_message' => t('CDSisis export file to xml validation encountered an error.'),
    );
    $batch['operations'][] = array('_importer_batch_first', array());
    $batch['operations'][] = array('_importer_batch_second', array());

// Tip the first domino.
    batch_set($batch);
  }
}

function fba_solution_pack_libcat_import_form_batch_import_validate($form, &$form_state) {
  $validobjectsfilename = '/tmp/FBAvalidobjects.xml';
  $mfnlistfilename = '/tmp/FBAmfn.txt';
  $form_state['form_has_errors'] = FALSE;

  if (!file_exists($validobjectsfilename)) {
    $form_state['form_has_errors'] = TRUE;
    form_set_error('', t($validobjectsfilename . ' cannot be opened.'));
  }
  if (!file_exists($mfnlistfilename)) {
    $form_state['form_has_errors'] = TRUE;
    form_set_error('', t($mfnlistfilename . ' cannot be opened.'));
  }
}

function _importer_batch_first(&$context) {
  drupal_set_message('Fired xml import/update operation.');
  $validobjectsfilename = '/tmp/FBAvalidobjects.xml';
  global $keywordarray;
  $keywordscsvfile = "docs/FBACatalogueKeyword-REPOSITORY.csv";

  initialiseCategoryMap();
  initialiseWaterAuthorityMap();

//1. Read in the index and keywords
  $path = drupal_get_path('module', 'fba_solution_pack_libcat_import');
  $csvfile = $path . '/' . $keywordscsvfile;

  $file_handle = fopen($csvfile, 'r');
  if ($file_handle) {
    while (!feof($file_handle)) {
      $keyworddeeparray[] = fgetcsv($file_handle, 1024);
    }
    fclose($file_handle);
    foreach ($keyworddeeparray as $kdpair) {
      if ($kdpair) {
        $kdkey = $kdpair[0];
        if (strlen($kdpair[1]) < 2) {
          $kdvalue = 'missing keyword definition';
        }
        else {
          $kdvalue = $kdpair[1];
        }
        $keywordarray[$kdkey] = $kdvalue;
      }
    }
  }
  else {
//TODO Don't think it'll ever reach here, as fopen throws errors itself if file didn't open correctly
    form_set_error(NULL, 'Could not process keywords file');
  }
//1.completed
//
//
  //Process the XML file
//use XMLReader as this doesn't drag the whole file in at once
  $xmlrdr = new XMLReader();

  if ($xmlrdr->open($validobjectsfilename)) {
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

                $mfn = $xml->getElementsByTagName('Mfn')->item(0)->nodeValue;

                //ignore mfn 1 as that's always a title record in cdsisis
                if ($mfn > 1) {
                  _process_record_node($xml);
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
      $recordreturntext = '';
      $xmlreadok = $xmlrdr->read();
    }
    $xmlrdr->close();

    $returnvalue = TRUE;
  }
  else {
    $current_set = &_batch_current_set();
    $current_set['success'] = FALSE;
    $context['results'][] = 'Batch 1: Could not open valid objects file.';
    $context['finished'] = 1.0;
    $returnvalue = FALSE;
  }
  return $returnvalue;
}

function _importer_batch_second(&$context) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');

  drupal_set_message('Fired mfn text batch operation.');
  $mfnlist = file('/tmp/FBAmfn.txt');

  if ($mfnlist) {
    $allmfnsandpids = _libcat_get_all_mfns();
  }
  else {
//failure to read the cdsisis mfn list
    $current_set = &_batch_current_set();
    $current_set['success'] = FALSE;
    $context['results'][] = 'Batch2: Error opening files.';
    $context['finished'] = 1.0;
    $returnvalue = FALSE;
  }

  $mfnobjstodelete = array();

  //loop through and compare both lists of mfns to see which objects need deleting, 
  // i.e. the object is in fedora, but not in the cdsisis export list.
  //sort arrays first
  array_multisort($mfnlist, SORT_ASC, SORT_NUMERIC);

  uasort($allmfnsandpids, "cmp");

  //move pointers to start of arrays
  reset($allmfnsandpids);
  reset($mfnlist);
  $fedoramfnobject = current($allmfnsandpids);
  $cdsisismfnobject = current($mfnlist);
  $keepprocessing = TRUE;
  while ($keepprocessing) {
    if (($fedoramfnobject != FALSE) && ($cdsisismfnobject != FALSE)) {
      $substring = substr($fedoramfnobject['mfn'], 6);
      $fedoramfnnumber = intval($substring);
      $cdsisismfnnumber = intval($cdsisismfnobject);

      if ($fedoramfnnumber == $cdsisismfnnumber) {
        $fedoramfnobject = next($allmfnsandpids);
        $cdsisismfnobject = next($mfnlist);
      }
      elseif ($fedoramfnnumber > $cdsisismfnnumber) {
        $cdsisismfnobject = next($mfnlist);
      }
      else {
        $mfnobjstodelete[] = $fedoramfnobject;
        $fedoramfnobject = next($allmfnsandpids);
      }
    }
    if (($fedoramfnobject == FALSE) || ($cdsisismfnobject == FALSE)) {
      if ($cdsisismfnobject == FALSE) {
        if ($fedoramfnobject == FALSE) {
          $keepprocessing = FALSE;
        }
        else {
          $mfnobjstodelete[] = $fedoramfnobject;
          $fedoramfnobject = next($allmfnsandpids);
          $substring = substr($fedoramfnobject['mfn'], 6);
          $fedoramfnnumber = intval($substring);
        }
      }
      elseif ($fedoramfnobject == FALSE) {
        $cdsisismfnobject = next($mfnlist);
        $cdsisismfnnumber = intval($cdsisismfnobject);
      }
    }
  }

  // code here to loop around $mfnstodelete, and remove the pids from fedora
  foreach ($mfnobjstodelete as $mfnobjtodelete) {
    //if this object has a pdf datastream, don't delete it.
    $pidofobjtodelete = $mfnobjtodelete['object'];

    if ($pidofobjtodelete) {
      $itemtodelete = getFedoraItem($pidofobjtodelete);

      if ($itemtodelete->exists()) {
        $dslist = $itemtodelete->get_datastreams_list_as_array();
      }
    }
  }
  return $returnvalue;
}

//function _importer_batch_third(&$context) {
//  $returnvalue = TRUE;
//  return $returnvalue;
//}

function _importer_batch_finished($success, $results, $operations) {
  if ($success) {
    $message = t('All CDSisis records processed.');
  }
  else {
    $message = t('Finished with error - ' . $results[0]);
  }
  drupal_set_message($message);
}

function checkValidElement($elementName, $docType) {
  $aDocTypeAElements = array('Mfn', 'ANALYTIC_LEVEL_author_s_', 'Author_role', 'Corporate_author_s_', 'Title', 'Serial_Journal_title', 'Date_of_publication', 'Collation', 'Other_pagination', 'ISBN', 'Annotation', 'Windermere_shelfmark', 'Dorset_shelfmark', 'Category', 'BL_index', 'Wat__Auth_', 'Indexing', 'Number_of_copies', 'Date_of_entry', 'Document_type', 'Keywords', 'Search_terms');
  $aDocTypeBElements = array('Mfn', 'MONOGRAPHIC_LEVEL_author_s_', 'Author_role', 'Corporate_author_s_', 'Title__monogr__', 'Edition', 'ISBN', 'Imprint__Place__Publ___Date_', 'Price', 'Pages', 'Other_pagination', 'Annotation', 'Windermere_shelfmark', 'Dorset_shelfmark', 'Category', 'BL_index', 'Wat__Auth_', 'Indexing', 'Number_of_copies', 'Date_of_entry', 'Document_type', 'Keywords', 'Search_terms');
  $aDocTypeCElements = array('Mfn', 'ANALYTIC_LEVEL_author_s_', 'Corporate_author_s_', 'Title', 'Monographic_source_author_s_', 'Corporate_source_author_s_', 'Title__monogr__', 'Imprint__Place__Publ___Date_', 'Extent_of_chapter', 'Other_pagination', 'Annotation', 'Windermere_shelfmark', 'Dorset_shelfmark', 'Category', 'BL_index', 'Wat__Auth_', 'Indexing', 'Number_of_copies', 'Date_of_entry', 'Document_type', 'Keywords', 'Search_terms');

  $aElements[] = NULL;
  if (strcmp($docType, 'A') == 0) {
    $aElements = $aDocTypeAElements;
  }
  else if (strcmp($docType, 'B') == 0) {
    $aElements = $aDocTypeBElements;
  }
  else if (strcmp($docType, 'C') == 0) {
    $aElements = $aDocTypeCElements;
  }
  else {
    return true;
  }

  foreach ($aElements as $sElement) {
    if (strcmp($elementName, $sElement) == 0) {
      return true;
    }
  }
  return false;
}

/*
 *
 * 
 * 
 * 
 *  
 */

function _process_record_node($xml) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $path = drupal_get_path('module', 'fba_solution_pack_libcat_import');
  global $keywordarray;
  //DEBUG line below so I can view the xml in text format!
  //$outputtest = $xml->saveXML();

  $sxe = simplexml_import_dom($xml);

  $mfn = (string) $sxe->Mfn;

  //Need to find Document Type
  $doctype = (string) $sxe->Document_type;
  $trimmeddoctype = _remove_fullstop($doctype);

  foreach ($sxe->children() as $sxnodes) {
    $sxnodename = $sxnodes->getName();

    if (!checkValidElement($sxnodename, $trimmeddoctype)) {
      storeInvalidElement($mfn, $trimmeddoctype, $sxnodename);
    }
  }

  //check for, and translate the  "Category_mapped" and "Water_authority_mapped" tags here
  if ($sxe->{'Category'}) {
    //Category might be an array
    $catvals = $sxe->{'Category'};
    $catvalcount = count($catvals);
    if ($catvalcount > 1) {
      //it's an array
      for ($i = 0; $i < $catvalcount; $i++) {
        $catval = (string) $sxe->{'Category'}[$i];

        $categorymapped = addCategory($catval);

        if ($categorymapped) {
          $sxe->addChild('Category_mapped', $categorymapped);
          //$sxe->{'Category_mapped'}[$i] = $categorymapped;
        }
        else {
          $sxe->addChild('Category_mapped', $catval);
          //$sxe->{'Category_mapped'}[$i] = $categorymapped;
        }
      }
    }
    else {
      $catval = (string) $sxe->{'Category'};
      $categorymapped = addCategory($catval);
      if ($categorymapped) {
        $sxe->addChild('Category_mapped', $categorymapped);
      }
      else {
        $sxe->addChild('Category_mapped', $catval);
      }
    }
  }

  if ($sxe->{'Wat__Auth_'}) {
    //might be an array
    $watvals = $sxe->{'Wat__Auth_'};
    $watvalcount = count($watvals);
    if ($watvalcount > 1) {
      //it's an array
      for ($i = 0; $i < $watvalcount; $i++) {
        $watval = (string) $sxe->{'Wat__Auth_'}[$i];
        $watauthmapped = addWaterAuthority($watval);

        if ($watauthmapped) {
          $sxe->addChild('Water_authority_mapped', $watauthmapped);
          //$sxe->{'Water_authority_mapped'}[$i] = $watauthmapped;
        }
        else {
          $sxe->addChild('Water_authority_mapped', $watval);
          //$sxe->{'Water_authority_mapped'}[$i] = $watauthmapped;
        }
      }
    }
    else {
      $watval = (string) $sxe->{'Wat__Auth_'};
      $watauthmapped = addWaterAuthority($watval);

      if ($watauthmapped) {
        $sxe->addChild('Water_authority_mapped', $watauthmapped);
      }
      else {
        $sxe->addChild('Water_authority_mapped', $watval);
      }
    }
  }

  if ($sxe->{'MONOGRAPHIC_LEVEL_author_s_'}) {
    //preprocess the value(s) to insert @@ between family and given names
    //The xslt will then split on @@

    $authvals = $sxe->{'MONOGRAPHIC_LEVEL_author_s_'};
    $authvalcount = count($authvals);
    if ($authvalcount > 1) {
      //it's an array
      for ($i = 0; $i < $authvalcount; $i++) {
        $authval = (string) $sxe->{'MONOGRAPHIC_LEVEL_author_s_'}[$i];
        $reformatted_auth = _insert_at_symbols_into_name($authval);
        $sxe->{'MONOGRAPHIC_LEVEL_author_s_'}[$i] = $reformatted_auth;
      }
    }
    else {
      //single value
      $authval = (string) $sxe->{'MONOGRAPHIC_LEVEL_author_s_'};
      $reformatted_auth = _insert_at_symbols_into_name($authval);
      $sxe->{'MONOGRAPHIC_LEVEL_author_s_'} = $reformatted_auth;
    }
  }

  if ($sxe->{'Monographic_source_author_s_'}) {
    //preprocess the value(s) to insert @@ between family and given names
    //The xslt will then split on @@

    $authvals = $sxe->{'Monographic_source_author_s_'};
    $authvalcount = count($authvals);
    if ($authvalcount > 1) {
      //it's an array
      for ($i = 0; $i < $authvalcount; $i++) {
        $authval = (string) $sxe->{'Monographic_source_author_s_'}[$i];
        $reformatted_auth = _insert_at_symbols_into_name($authval);
        $sxe->{'Monographic_source_author_s_'}[$i] = $reformatted_auth;
      }
    }
    else {
      //single value
      $authval = (string) $sxe->{'Monographic_source_author_s_'};
      $reformatted_auth = _insert_at_symbols_into_name($authval);
      $sxe->{'Monographic_source_author_s_'} = $reformatted_auth;
    }
  }

  if ($sxe->{'ANALYTIC_LEVEL_author_s_'}) {
    //preprocess the value(s) to insert @@ between family and given names
    //The xslt will then split on @@

    $authvals = $sxe->{'ANALYTIC_LEVEL_author_s_'};
    $authvalcount = count($authvals);
    if ($authvalcount > 1) {
      //it's an array
      for ($i = 0; $i < $authvalcount; $i++) {
        $authval = (string) $sxe->{'ANALYTIC_LEVEL_author_s_'}[$i];
        $reformatted_auth = _insert_at_symbols_into_name($authval);
        $sxe->{'ANALYTIC_LEVEL_author_s_'}[$i] = $reformatted_auth;
      }
    }
    else {
      //single value
      $authval = (string) $sxe->{'ANALYTIC_LEVEL_author_s_'};
      $reformatted_auth = _insert_at_symbols_into_name($authval);
      $sxe->{'ANALYTIC_LEVEL_author_s_'} = $reformatted_auth;
    }
  }

  if ($sxe->{'Author_role'}) {
    //preprocess the value(s) to map them
    $authrolevals = $sxe->{'Author_role'};
    $authvalcount = count($authrolevals);
    if ($authvalcount > 1) {
      //it's an array
      for ($i = 0; $i < $authvalcount; $i++) {
        $authroleval = (string) $sxe->{'Author_role'}[$i];
        switch (trim($authroleval)) {
          case 'ed':
          case 'ed.':
          case 'Ed.':
          case 'eds':
          case 'eds.':
          case 'Organiser': {
              $reformatted_authrole = 'Editor';
              break;
            }
          case 'comp.': {
              $reformatted_authrole = 'Compiler';
              break;
            }
          case 'transl.': {
              $reformatted_authrole = 'Translator';
              break;
            }
          default : {
              $reformatted_authrole = 'Author';
              break;
            }
        }
        $sxe->{'Author_role'}[$i] = $reformatted_authrole;
      }
    }
    else {
      $authroleval = (string) $sxe->{'Author_role'};
      switch (trim($authroleval)) {
        case 'ed':
        case 'ed.':
        case 'Ed.':
        case 'eds':
        case 'eds.':
        case 'Organiser': {
            $reformatted_authrole = 'Editor';
            break;
          }
        case 'comp.': {
            $reformatted_authrole = 'Compiler';
            break;
          }
        case 'transl.': {
            $reformatted_authrole = 'Translator';
            break;
          }
        default : {
            $reformatted_authrole = 'Author';
            break;
          }
      }
      $sxe->{'Author_role'} = $reformatted_authrole;
    }
  }

  //pass the xml through the xslt stylesheet
  $xsl = new DOMDocument();
  $xsl->load($path . '/xslt/mods' . $trimmeddoctype . '.xslt');

  $stylesheetxml = new DOMDocument();
  $sxeasxml = $sxe->asXML();
  $stylesheetxml->loadXML($sxeasxml);

  $proc = new XSLTProcessor();
  $proc->importStylesheet($xsl);
  $mods = $proc->transformToDoc($stylesheetxml);
  //xslt processing finished

  $mfnandpid = _libcat_get_specific_mfn($mfn);
  $pid = $mfnandpid[0]['object'];
  $mods_xml = $mods->saveXML();
  $modssxe = new SimpleXMLElement($mods_xml);

  if ($pid) { //check if it's an update or add
    $item = getFedoraItem($pid);

    if ($item->exists()) {
      //fedora item to be updated
      //but don't include keywords/search terms/indexing
      $sxexml = $modssxe->asXML();
      _update_fedora_object($mfn, $trimmeddoctype, $sxexml);
    }
  }
  else {
    //fedora item to be added
    //Get Indexing value, as it has to be translated to keyword(s)
    $domindexing = $xml->getElementsByTagName('Indexing');
    foreach ($domindexing as $el) {
      $indexing[] = $el->nodeValue;
    }
    $keywordstoadd = array(); //initialise
    if ($indexing) {
      foreach ($indexing as $indexkey) {
        $keyval = strtolower(_remove_fullstop($indexkey));
        if (($keyval != '0') && ($keywordarray[$keyval])) {
          $keywords[] = $keywordarray[$keyval];
        }
        else {
          //TODO: report this as error?
        }
      }

      if ($keywords) {
        //$keywords will need adding manually to the datastream
        foreach ($keywords as $keywordswithsemicolons) {
          $explodedkeywords = explode(';', $keywordswithsemicolons);
          foreach ($explodedkeywords as $individualkeyword) {
            //trim whitespace and capitalise the first letter of string
            $trimmed = trim($individualkeyword);
            if (!empty($trimmed)) { //sometimes there is a whitespace after the last semicolon
              $allkeywords[] = ucfirst($trimmed);
            }
          }
        }
        $keywordstoadd = array_unique($allkeywords);
      }
    }
    // if $keywordstoadd != false, then there are values to add as <subject>
    if ($keywordstoadd) {
      foreach ($keywordstoadd as $kw) {
        $sxesubject = $modssxe->addChild('mods:subject');
        $sxesubject->addAttribute('authority', 'fba');
        $sxesubject->addChild('mods:topic', $kw);
      }
    }

    $sxexml = $modssxe->asXML();
    _write_fedora_object($trimmeddoctype, $sxexml);
  }

  return;
}

function _write_fedora_object($doctype, $mods) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'api/ObjectHelper');

  $contentmodelformpath = drupal_get_path('module', 'islandora_content_model_forms');

  //constants for this function
  $namespace = 'fbalc';
  $collection_pid = 'info:fedora/' . $namespace . ':collection';

  $modsdsid = 'MODS';
  $modsdslabel = 'MODS Record';
  $dcdsid = 'DC';
  $dcdslabel = 'Dublin Core';

  //Add a new fedora object.
  // add this object to fedora via the content model
  $nextavailablepid = fedora_item::get_next_PID_in_namespace($namespace);
  global $user;

  $modsxml = new DOMDocument();
  $modsxml->loadXML($mods);

  $modsxmlxpath = new DOMXPath($modsxml);
  $titlenodes = $modsxmlxpath->query('//mods:mods/mods:titleInfo');
  $title = $titlenodes->item(0)->nodeValue;
  if (!$title) {
    $title = '<title missing>';
  }
  $item = fedora_item::ingest_new_item($nextavailablepid, 'A', $title, $user->name);
  $item->add_relationship('isMemberOfCollection', $collection_pid, RELS_EXT_URI);
  switch ($doctype) {
    case A: {
        $item->add_relationship('hasModel', 'fba:library_article_cmodel', FEDORA_MODEL_URI);
        break;
      }
    case B: {
        $item->add_relationship('hasModel', 'fba:library_monograph_cmodel', FEDORA_MODEL_URI);
        break;
      }
    case C: {
        $item->add_relationship('hasModel', 'fba:library_chapter_cmodel', FEDORA_MODEL_URI);
        break;
      }
  }
  $thumbnail = drupal_get_path('module', 'Fedora_Repository') . '/images/Crystal_Clear_filesystem_folder_grey.png';
  $item->add_datastream_from_file($thumbnail, 'TN');
  $item->add_datastream_from_string($mods, $modsdsid, $modsdslabel, 'text/xml', 'X');

  //pass the xml through the fba mods to dc xslt stylesheet
  $fbamodsdcxsl = new DOMDocument();
  $fbamodsdcxsl->load($contentmodelformpath . '/transforms/fba_mods_to_dc.xsl');

  $proc = new XSLTProcessor();
  $modsdom = new DomDocument();
  $modsdom->loadXML(trim($mods));
  $proc->importStylesheet($fbamodsdcxsl);
  $dcdom = $proc->transformToDoc($modsdom);
  $dc_xml = $dcdom->saveXML();

  $datastreams = $item->get_datastreams_list_as_array();
  if (array_key_exists($dcdsid, $datastreams)) {
    $item->modify_datastream_by_value($dc_xml, $dcdsid, $dcdslabel, 'text/xml');
  }
  else {
    $item->add_datastream_from_string($dc_xml, $dcdsid, $dcdslabel, 'text/xml', 'X');
  }
  return;
}

function _update_fedora_object($mfn, $doctype, $mods) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'api/ObjectHelper');

  $contentmodelformpath = drupal_get_path('module', 'islandora_content_model_forms');

//constants for this function
  $namespace = 'fbalc';
  $collection_pid = 'info:fedora/' . $namespace . ':collection';

  $modsdsid = 'MODS';
  $modsdslabel = 'MODS Record';
  $dcdsid = 'DC';
  $dcdslabel = 'Dublin Core';

  $rcsxpath = '//mods:mods/mods:recordInfo/mods:recordContentSource';
  $mfnxpath = '//mods:mods/mods:identifier[@type="mfn"]';
  $authxpath = '//mods:mods/mods:name[@type="personal"]';
  $doctypexpath = '//mods:mods/mods:note[@type="doc type"]';
  $torxpath = '//mods:mods/mods:typeOfResource';
  $genrexpath = '//mods:mods/mods:genre';
  $issxpath = '//mods:mods/mods:originInfo/mods:issuance';
  $hixpath = '//mods:mods/mods:relatedItem[@type="host"]/mods:originInfo/mods:issuance';
  $hdixpath = '//mods:mods/mods:relatedItem[@type="host"]/mods:originInfo/mods:dateIssued';
  $titlexpath = '//mods:mods/mods:titleInfo';
  $hosttitlexpath = '//mods:mods/mods:relatedItem[@type="host"]/mods:titleInfo';
  $dopxpath = '//mods:mods/mods:extension/mods:dateOfPublication';
  $indexingxpath = '//mods:mods/mods:classification[@authority="fba" and @displayLabel="FBA Legacy Keyword Strings"]';
//  $keywordxpath = '//mods:mods/mods:subject[@authority="fba"]/mods:topic';
  $volnumxpath = '//mods:mods/mods:relatedItem[@type="host"]/mods:part/mods:detail[@type="volume"]';
  $partnumxpath = '//mods:mods/mods:relatedItem[@type="host"]/mods:part/mods:detail[@type="issue"]';
  $pagesnumxpath = '//mods:mods/mods:relatedItem[@type="host"]/mods:part/mods:extent[@unit="pages"]';
//end constants

  $mfnandpid = _libcat_get_specific_mfn($mfn);
  $pid = $mfnandpid[0]['object'];
  if ($pid) {
    $item = getFedoraItem($pid);
    //Do an update on any of the fields only that have changed, so that newer data in fedora isn't overwritten.
    $fedoramodsds = (string) '<?xml version="1.0"?>' . $item->get_datastream_dissemination($modsdsid);
    $fedoramodsxml = new DOMDocument();
    $fedoramodsxml->loadXML($fedoramodsds);
    $fedoramodsxp = new DOMXPath($fedoramodsxml);

    $cdsmodsxml = new DOMdocument();
    $cdsmodsxml->loadXML($mods);
    $cdsmodsxp = new DOMXPath($cdsmodsxml);

    $finalxml = new DOMdocument();
    $finalxml->loadXML($fedoramodsds);
    $finalmodsxp = new DOMXPath($finalxml);

//////////////////
    $mfnarraydiff = _xpathdiff($mfnxpath, $cdsmodsxp, $fedoramodsxp);
//////////////////
//        mods:mods/mods:name[@type="personal"]/mods:namePart[@type="given"]/
//        mods:mods/mods:name[@type="personal"]/mods:namePart[@type="family"]/
    $fnames = $fedoramodsxp->query('//mods:mods/mods:name[@type="personal"]/mods:namePart');
    $fnamevals = array();
    for ($i = 0; $i < $fnames->length; $i = $i + 2) { //+2 as given and family are in pairs
      $fnamevals[] = $fnames->item($i)->nodeValue . '@' . $fnames->item($i + 1)->nodeValue;
    }

    $cnames = $cdsmodsxp->query('//mods:mods/mods:name[@type="personal"]/mods:namePart');
    $cnamevals = array();
    for ($i = 0; $i < $cnames->length; $i = $i + 2) { //+2 as given and family are in pairs
      $cnamevals[] = $cnames->item($i)->nodeValue . '@' . $cnames->item($i + 1)->nodeValue;
    }

    $namearraydiff = array_diff($cnamevals, $fnamevals);
    $rcsarraydiff = _xpathdiff($rcsxpath, $cdsmodsxp, $fedoramodsxp);
    $rtarraydiff = _xpathdiff('//mods:mods/mods:name[@type="personal"]/mods:role/mods:roleTerm[@authority="marcrelator"]', $cdsmodsxp, $fedoramodsxp);
    $doctypearraydiff = _xpathdiff($doctypexpath, $cdsmodsxp, $fedoramodsxp);
    $titlearraydiff = _xpathdiff('//mods:mods/mods:titleInfo/mods:title', $cdsmodsxp, $fedoramodsxp);
    $issuancearraydiff = _xpathdiff('//mods:mods/mods:originInfo/mods:issuance', $cdsmodsxp, $fedoramodsxp);
    $torarraydiff = _xpathdiff($torxpath, $cdsmodsxp, $fedoramodsxp);
    $genrearraydiff = _xpathdiff($genrexpath, $cdsmodsxp, $fedoramodsxp);
    $hostissuancearraydiff = _xpathdiff($hixpath, $cdsmodsxp, $fedoramodsxp);
    $diarraydiff = _xpathdiff($hdixpath, $cdsmodsxp, $fedoramodsxp);
    $hosttitlearraydiff = _xpathdiff('//mods:mods/mods:relatedItem[@type="host"]/mods:titleInfo/mods:title', $cdsmodsxp, $fedoramodsxp);
    $volnumarraydiff = _xpathdiff('//mods:mods/mods:relatedItem[@type="host"]/mods:part/mods:detail[@type="volume"]/mods:number', $cdsmodsxp, $fedoramodsxp);
    $partnumarraydiff = _xpathdiff('//mods:mods/mods:relatedItem[@type="host"]/mods:part/mods:detail[@type="issue"]/mods:number', $cdsmodsxp, $fedoramodsxp);
    $pagesnumarraydiff = _xpathdiff('//mods:mods/mods:relatedItem[@type="host"]/mods:part/mods:extent[@unit="pages"]/mods:list', $cdsmodsxp, $fedoramodsxp);
    $doparraydiff = _xpathdiff($dopxpath, $cdsmodsxp, $fedoramodsxp);
//      $kwarraydiff = _xpathdiff($indexingxpath, $cdsmodsxp, $fedoramodsxp);
//      $topicarraydiff = _xpathdiff($keywordxpath, $cdsmodsxp, $fedoramodsxp);

    if ((count($mfnarraydiff) > 0) ||
        (count($namearraydiff) > 0) ||
        (count($rcsarraydiff) > 0) ||
        (count($rtarraydiff) > 0) ||
        (count($doctypearraydiff) > 0) ||
        (count($titlearraydiff) > 0) ||
        (count($issuancearraydiff) > 0) ||
        (count($torarraydiff) > 0) ||
        (count($genrearraydiff) > 0) ||
        (count($hostissuancearraydiff) > 0) ||
        (count($diarraydiff) > 0) ||
        (count($hosttitlearraydiff) > 0) ||
        (count($volnumarraydiff) > 0) ||
        (count($partnumarraydiff) > 0) ||
        (count($pagesnumarraydiff) > 0) ||
//          (count($kwarraydiff) > 0) ||
//          (count($topicarraydiff) > 0) ||
        (count($doparraydiff) > 0)) {

//there's at least one difference, so remove all the sections from the output mods, and add the new stuff in.
//MFN 
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($mfnxpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }
//Hard coded record content source
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($rcsxpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }
//Author
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($authxpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//Document Type
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($doctypexpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//Type of resource
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($torxpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//Genre
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($genrexpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//Issuance
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($issxpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//Host Issuance
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($hixpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//Host Date Issued
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($hdixpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//Title
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($titlexpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//Host Title
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($hosttitlexpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//Date of publication
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($dopxpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//Indexing
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($indexingxpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//keywords
//remove the whole element from the output xml
//        $xpathnodes = $finalmodsxp->query($keywordxpath);
//        foreach ($xpathnodes as $node) {
//remove old one
//          $removednode = $node->parentNode->removeChild($node);
//        }
//volnum
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($volnumxpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//partnum
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($partnumxpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//pagesnum 
//remove the whole element from the output xml
      $xpathnodes = $finalmodsxp->query($pagesnumxpath);
      foreach ($xpathnodes as $node) {
//remove old one
        $removednode = $node->parentNode->removeChild($node);
      }

//remove empty elements such after the above deletions
//  <mods:recordInfo></mods:recordInfo>
//  <mods:originInfo></mods:originInfo>
//  <mods:relatedItem type="host">
//    <mods:originInfo></mods:originInfo>
//    <mods:part></mods:part>
// </mods:relatedItem>
//<mods:extension></mods:extension>

      $emptyelements = $finalmodsxp->query('//mods:mods/mods:recordInfo[not(node()) or normalize-space() = ""]');
      foreach ($emptyelements as $emptynode) {
        $removednode = $emptynode->parentNode->removeChild($emptynode);
      }
      $emptyelements = $finalmodsxp->query('//mods:mods/mods:originInfo[not(node()) or normalize-space() = ""]');
      foreach ($emptyelements as $emptynode) {
        $emptynode->parentNode->removeChild($emptynode);
      }
      $emptyelements = $finalmodsxp->query('//mods:mods/mods:relatedItem[@type="host"]/mods:originInfo[not(node()) or normalize-space() = ""]');
      foreach ($emptyelements as $emptynode) {
        $emptynode->parentNode->removeChild($emptynode);
      }
      $emptyelements = $finalmodsxp->query('//mods:mods/mods:relatedItem[@type="host"]/mods:part[not(node()) or normalize-space() = ""]');
      foreach ($emptyelements as $emptynode) {
        $emptynode->parentNode->removeChild($emptynode);
      }
      $emptyelements = $finalmodsxp->query('//mods:mods/mods:relatedItem[@type="host" and (not(node()) or normalize-space() = "")]');
      foreach ($emptyelements as $emptynode) {
        $emptynode->parentNode->removeChild($emptynode);
      }
      $emptyelements = $finalmodsxp->query('//mods:mods/mods:extension[not(node()) or normalize-space() = ""]');
      foreach ($emptyelements as $emptynode) {
        $emptynode->parentNode->removeChild($emptynode);
      }
      //patch in the new mods xml elements from cdsisis
      $simplexmlFinal = simplexml_load_string($finalxml->saveXML());
      $simplexmlCDS = simplexml_load_string($mods);
      simplexml_merge($simplexmlFinal, $simplexmlCDS);

      $outputFinal = $simplexmlFinal->asXML();

      $item->modify_datastream_by_value($outputFinal, $modsdsid, $modsdslabel, 'text/xml');
//pass the xml through the fba mods to dc xslt stylesheet
      $fbamodsdcxsl = new DOMDocument();
      $fbamodsdcxsl->load($contentmodelformpath . '/transforms/fba_mods_to_dc.xsl');

      $proc = new XSLTProcessor();
      $modsdom = new DomDocument();
      $modsdom->loadXML(trim($mods));
      $proc->importStylesheet($fbamodsdcxsl);
      $dcdom = $proc->transformToDoc($modsdom);
      $dc_xml = $dcdom->saveXML();

      $datastreams = $item->get_datastreams_list_as_array();
      if (array_key_exists($dcdsid, $datastreams)) {
        $item->modify_datastream_by_value($dc_xml, $dcdsid, $dcdslabel, 'text/xml');
      }
      else {
        $item->add_datastream_from_string($dc_xml, $dcdsid, $dcdslabel, 'text/xml', 'X');
      }

//need to get the label from title first to set the foxml label
      $dctitle = _xpathvalue('//mods:mods/mods:titleInfo/mods:title', $cdsmodsxp);
      $item->modify_object($dctitle); // Set the label
    }

    return;
  }
}

function initialiseCategoryMap() {
  global $categoryArray;

  $categoryArray['B'] = 'Bibliography';
  $categoryArray['BK'] = 'Book';
  $categoryArray['BL'] = 'British List';
  $categoryArray['Bl'] = 'British List';
  $categoryArray['Environment Agency Archives North West'] = 'Environment Agency Archives North West';
  $categoryArray['Environment Agency Archives North West Region'] = 'Environment Agency Archives North West';
  $categoryArray['Environment Agency Archives South West'] = 'Environment Agency Archives South West';
  $categoryArray['Environment Agency South West'] = 'Environment Agency South West';
  $categoryArray['EU'] = 'European Union';
  $categoryArray['F'] = 'Fritsch';
  $categoryArray['HCL'] = 'Hilda Canter-Lund Collection';
  $categoryArray['HL'] = 'Hutton Library';
  $categoryArray['JL'] = 'John Lund Collection';
  $categoryArray['1K0'] = 'Ignore';
  $categoryArray['L'] = 'Language';
  $categoryArray['LE'] = 'Leedale Collection';
  $categoryArray['M'] = 'Microfiche / Microfilm';
  $categoryArray['MEV'] = 'Margaret Elizabeth Varley Collection';
  $categoryArray['R'] = 'Review';
  $categoryArray['RL'] = 'River Lab';
  $categoryArray['SP'] = 'FBA Staff Papers';
  $categoryArray['TH'] = 'Thesis';
  $categoryArray['C1.17A(40)'] = 'Ignore';
  $categoryArray['4DE0'] = 'Ignore';
  $categoryArray['BK/'] = 'Book';
  $categoryArray['7g0'] = 'Ignore';
  $categoryArray['6ED7'] = 'Ignore';
}

function initialiseWaterAuthorityMap() {
  global $waterAuthMap;
  $waterAuthMap['NNWA'] = 'North West Water Authority';
  $waterAuthMap['NWA'] = 'Northumbria Water Authority';
  $waterAuthMap['NWWA'] = 'North West Water Authority';
  $waterAuthMap['NWWWA'] = 'North West Water Authority';
  $waterAuthMap['NYA'] = 'Yorkshire Water Authority';
  $waterAuthMap['SRWA'] = 'Seven Trent Water Authority';
  $waterAuthMap['STWA'] = 'Seven Trent Water Authority';
  $waterAuthMap['SWA'] = 'Southern Water Authority';
  $waterAuthMap['SWWA'] = 'South West Water Authority';
  $waterAuthMap['TWA'] = 'Thames Water Authority';
  $waterAuthMap['WEWA'] = 'Welsh Water Authority';
  $waterAuthMap['WWA'] = 'Wessex Water Authority';
  $waterAuthMap['YWA'] = 'Yorkshire Water Authority';
  $waterAuthMap['British isles'] = 'British Isles';
  $waterAuthMap['NWWA;'] = 'North West Water Authority';
  $waterAuthMap['AWA'] = 'Anglian Water Authority';
  $waterAuthMap['Ireland'] = 'Ireland';
  $waterAuthMap['Isle of Man'] = 'Isle of Man';
  $waterAuthMap['British Isles'] = 'British Isles';
  $waterAuthMap['Scotland'] = 'Scotland';
  $waterAuthMap['SCotland'] = 'Scotland';
  $waterAuthMap['England'] = 'England';
  $waterAuthMap['British Isles;'] = 'British Isles';
  $waterAuthMap['Wales'] = 'Wales';
  $waterAuthMap['Scilly'] = 'Scilly';
  $waterAuthMap['9EA4'] = 'Ignore';
  $waterAuthMap['Scotland: WEWA'] = 'Scotland; Welsh Water Authority';
  $waterAuthMap['Galway'] = 'Galway';
  $waterAuthMap['Berkshire: lakes'] = 'Ignore';
  $waterAuthMap['River Gwash'] = 'Ignore';
  $waterAuthMap['1C0'] = 'Ignore';
  $waterAuthMap['7B3'] = 'Ignore';
  $waterAuthMap['4A0'] = 'Ignore';
  $waterAuthMap['River Glen'] = 'Ignore';
  $waterAuthMap['4ec0'] = 'Ignore';
}

function addCategory($categoryFromRecord) {
  global $categoryArray;
  $categorymapped = NULL;
  $reformattedcat = _remove_fullstop($categoryFromRecord);
  if (array_key_exists($reformattedcat, $categoryArray)) {
    $foundcategory = $categoryArray[$reformattedcat];
  }
  if (($foundcategory ) && (strcmp($foundcategory, 'Ignore') != 0)) {
    $categorymapped = $foundcategory;
  }
  return $categorymapped;
}

function addWaterAuthority($waterAuthorityFromRecord) {
  global $waterAuthMap;

  $waterauthoritymapped = NULL;
  $reformattedwa = _remove_fullstop($waterAuthorityFromRecord);
  if (array_key_exists($reformattedwa, $waterAuthMap)) {
    $foundwatauth = $waterAuthMap[$reformattedwa];
  }
  if (($foundwatauth ) && (strcmp($foundwatauth, 'Ignore') != 0)) {
    $waterauthoritymapped = $foundwatauth;
  }
  return $waterauthoritymapped;
}

function _xpathdiff($xpath, $cdsmodsxp, $fedoramodsxp) {
  $fed = $fedoramodsxp->query($xpath);
  $fedvals = array();
  for ($i = 0; $i < $fed->length; $i++) {
    $fedvals[] = $fed->item($i)->nodeValue;
  }

  $cds = $cdsmodsxp->query($xpath);
  $cdsvals = array();
  for ($i = 0; $i < $cds->length; $i++) {
    $cdsvals[] = $cds->item($i)->nodeValue;
  }

  return array_diff($cdsvals, $fedvals);
}

function _xpathvalue($xpath, $modsxp) {
  $queryvals = $modsxp->query($xpath);
  $results = array();
  for ($i = 0; $i < $queryvals->length; $i++) {
    $results[] = $queryvals->item($i)->nodeValue;
  }

  return $results[
      0];
}

function simplexml_merge(SimpleXMLElement &$xml1, SimpleXMLElement $xml2) {
// convert SimpleXML objects into DOM ones
  $dom1 = new DomDocument();
  $dom2 = new DomDocument();
  $dom1->loadXML($xml1->asXML());
  $dom2->loadXML($xml2->asXML());
// pull all child elements of second XML
  $xpath = new domXPath($dom2);
  $xpathQuery = $xpath->query('/*/*');
  for ($i = 0; $i < $xpathQuery->length; $i++) {
//and pump them into first one
    $dom1->documentElement->appendChild($dom1->importNode($xpathQuery->item($i), true));
  }
  $xml1 = simplexml_import_dom($dom1); //pass by reference
}

function storeInvalidElement($mfn, $docType, $elementName) {
  global $nTotalsInvalidElements;
  global $fInvalidElements;
  global $sInvalidElementsFile;
//System . out . println("Invalid Element (See Invalid Element xml file for details):");

  try {
    if ($nTotalsInvalidElements == 0) {
      createInvalidElementsFile();
    }
    $nTotalsInvalidElements++;
    fwrite($fInvalidElements, "Invalid Element. MFN: " + $mfn + " DocType:" + $docType + " Element: " + $elementName + "\n");
  } catch (Exception $ex) {
    //System . out . println("Could not write to error file");
  }
}

function createInvalidElementsFile() {
  global $sInvalidElementsFile;
  global $fInvalidElements;


  $sInvalidElementsFile = '/tmp/invalidelements.txt';
  try {
    $fInvalidElements = fopen($sInvalidElementsFile, 'w');
  } catch (FileNotFoundException $e) {
    $retval = $e;
//System . out . println("Errors file cannot be created");
    return;
  }
}

function _insert_at_symbols_into_name($authval) {
  //split the $authval into family and given
  //search for first fullstop
  //search leftwards for space, split string there.
  $strlen = strlen($authval);
  $fullstoppos = strpos($authval, '.');
  $spacepos = strrpos($authval, ' ', $fullstoppos - $strlen); //subtraction gives a negative value, therefore searching leftwards
  $familyname = substr($authval, 0, $spacepos);
  $givenname = substr($authval, $spacepos + 1); //+1 to ignore space
  //recombine with @@ as separator
  return $familyname . '@@' . $givenname;
}

// Comparison function
function cmp($a, $b) {
  $mfna = intval(substr($a[mfn], 6));
  $mfnb = intval(substr($b[mfn], 6));
  if ($mfna == $mfnb) {
    return 0;
  }
  return ($mfna < $mfnb) ? -1 : 1;
}