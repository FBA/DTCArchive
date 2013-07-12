<?php

function _validate_record_node($xml) {
  $path = drupal_get_path('module', 'fba_solution_pack_libcat_import');

  $recordhaserror = FALSE;
  $returnpagetext = '';

  $mfn = $xml->getElementsByTagName('Mfn')->item(0)->nodeValue;

  //Need to find Document Type
  $domdoctype = $xml->getElementsByTagName('Document_type');
  foreach ($domdoctype as $el) {
    $doctype = $el->nodeValue;
  }
  $pos = strpos($doctype, '.');
  if ($pos) {
    $trimmeddoctype = substr($doctype, 0, $pos);
  }
  else {
    $trimmeddoctype = trim($doctype);
  }

  //and check that it's valid. 'T' isn't, for example
  $validdoctypes = array('A', 'B', 'C');
  if (in_array($trimmeddoctype, $validdoctypes)) {
    //allow us to catch errors from schema validation
    //TODO this messes up with drupal batches which "switch off" the php timeout
    libxml_use_internal_errors(true);
    $xmlok = TRUE;//$xml->schemaValidate($path . '/xsd/FBA' . $trimmeddoctype . '.xsd');

    if (!$xmlok) {
      $recordhaserror = TRUE;

      $errors = libxml_get_errors();
      foreach ($errors as $error) {
        $errmessage = rtrim($error->message); //removes newline as that messes up the line formatting later
        $returnpagetext .= 'XML error ' . $errmessage;
      }
      libxml_clear_errors();
    }
    libxml_use_internal_errors(false);
  }
  else {
    //report doctype error
    $recordhaserror = TRUE;
    $returnpagetext .= 'Document Type is "' . $trimmeddoctype . '"';
  }

  //Check the Date of publication values, only valid format is YYYY for now!
  $doporiginal = $xml->getElementsByTagName('Date_of_publication')->item(0)->nodeValue;

  if ($doporiginal) {
    if (substr($doporiginal, 0, 4) == 'n.d.') {
      $dop = trim($doporiginal);
    }
    else {
      $dop = rtrim($doporiginal, '.  '); //fullstop and 2 spaces added by export from cdsisis ;
    }
  }

  if ($dop) {
    $yearfromdop = _date_convert($dop);
    if (!$yearfromdop) {
      $recordhaserror = TRUE;
      $returnpagetext .= 'Date of publication error "' . rtrim($doporiginal, "\n") . '"';
    }
    else {
      if ((is_numeric($yearfromdop)) && (strlen($yearfromdop) == 4)) {
        //it's a rudimental check, but it's ok for now
        if ((intval($yearfromdop) <= 1686 ) || (intval($yearfromdop) >= 2014 )) {
          $recordhaserror = TRUE;
          $returnpagetext .= 'Date of publication error "' . rtrim($doporiginal, "\n") . '"';
        }
      }
      else {
        //$recordhaserror = TRUE;
        //$returnpagetext .= "\n" . 'Date of publication error "' . rtrim($doporiginal, "\n") . '"';
      }
    }
  }
  //Check the Imprint b values, only valid format is YYYY for now!
  //No need to strip the fullstop and space from the end of the strings.
  $ippdb = $xml->getElementsByTagName('Imprint__Place__Publ___Date__b')->item(0)->nodeValue;

  if ($ippdb) {
    $yearfromippdb = _date_convert($ippdb);
    if (!$yearfromippdb) {
      $recordhaserror = TRUE;
      $returnpagetext .= 'Imprint__Place__Publ___Date__b error "' . rtrim($ippdb, "\n") . '"';
    }
    else {
      if ((is_numeric($yearfromippdb)) && (strlen($yearfromippdb) == 4)) {
        //it's a rudimental check, but it's ok for now
        if ((intval($yearfromippdb) <= 1686 ) || (intval($yearfromippdb) >= 2014 )) {
          $recordhaserror = TRUE;
          $returnpagetext .= 'Imprint__Place__Publ___Date__b error "' . rtrim($yearfromippdb, "\n") . '"';
        }
      }
      else {
        //$recordhaserror = TRUE;
        //$returnpagetext .= "\n" . 'Imprint__Place__Publ___Date__b error "' . rtrim($yearfromippdb, "\n") . '"';
      }
    }
  }

  //ANALYTIC_LEVEL_author_s_	MONOGRAPHIC_LEVEL_author_s_	Monographic_source_author_s_
  //Reject anything not in format of 
  //  Surname<space>A.B.C. (ie repeating groups of initial and fullstop)

  $AllAlas = $xml->getElementsByTagName('ANALYTIC_LEVEL_author_s_');
  foreach ($AllAlas as $alas) {
    $alasval = $alas->nodeValue;
    if (preg_match('/[a-zA-Z-],?\s([a-zA-Z]\.)+\s?/', $alasval, $matches) == FALSE) {
      $recordhaserror = TRUE;
      $returnpagetext .= 'ANALYTIC_LEVEL_author_s_ name format error "' . rtrim($alasval, "\n") . '"';
    }
  }

  $AllMlas = $xml->getElementsByTagName('MONOGRAPHIC_LEVEL_author_s_');
  foreach ($AllMlas as $mlas) {
    $mlasval = $mlas->nodeValue;
    if (preg_match('/[a-zA-Z-],?\s([a-zA-Z]\.)+\s?/', $mlasval, $matches) == FALSE) {
      $recordhaserror = TRUE;
      $returnpagetext .= 'MONOGRAPHIC_LEVEL_author_s_ format error "' . rtrim($mlasval, "\n") . '"';
    }
  }

  $AllMsas = $xml->getElementsByTagName('Monographic_source_author_s_');
  foreach ($AllMsas as $msas) {
    $msasval = $msas->nodeValue;
    if (preg_match('/[a-zA-Z-],?\s([a-zA-Z]\.)+\s?/', $msasval, $matches)== FALSE) {
      $recordhaserror = TRUE;
      $returnpagetext .= 'Monographic_source_author_s_ format error "' . rtrim($msasval, "\n") . '"';
    }
  }


  //Date of Entry being ignored for now
  //Check the Date of Entry values, format is NOV95 ie MMMYY (MMM is case insensitive
  //  $doe = _remove_fullstop($xml->getElementsByTagName('Date_of_entry')->item(0)->nodeValue);
  //  $doemonth = substr($doe, 0, 3);
  //  $doeyear = substr($doe, 3);
  //  $validmonths = array('JAN', 'FEB', 'MAR', 'APR', 'MAY', 'JUN', 'JUL', 'AUG', 'SEP', 'OCT', 'NOV', 'DEC');
  //  if ((!in_array(strtoupper($doemonth), $validmonths)) || (!is_numeric($doeyear))) {
  //    $recordhaserror = TRUE;
  //    $returnpagetext .= 'Date of Entry invalid - "' . $doe . '"<br/>';
  //  }

  $domrecordelements = $xml->getElementsByTagName('RECORD');
  foreach ($domrecordelements as $el) {
    $subelements = array();
    if ($el->childNodes->length) {
      foreach ($el->childNodes as $i) {
        if (substr($i->nodeName, 0, 1) != '#') {
          $subelements[$i->nodeName] = $i->nodeValue;
        }
      }
    }
  }
  $validelementerror = array();
  foreach ($subelements as $subelementkey => $subelementvalue) {
    if (!checkValidElement($subelementkey, $trimmeddoctype)) {
      $recordhaserror = TRUE;
      $validelementerror[] = '"' . $subelementkey . '"';
    }
    else {
      //not a valid type
    }
  }
  //this is a workaround to ensure each error gets an mfn line below
  if (count($validelementerror) > 0) {
    $returnpagetext .= 'Invalid field(s) for type "' . $trimmeddoctype . '"  - ';

    foreach ($validelementerror as $veetext) {
      $returnpagetext .= $veetext;
    }
  }

  if ($recordhaserror) {
    $returnpage = $mfn . ',' . $returnpagetext . "\n";
    return $returnpage;
  }
  else {
    return FALSE;
  }
}

function _date_convert($indate) {
  $numbers = array("1", "2", "3", "4", "5", "6", "7", "8", "9", "0");

  //find the date format by converting all numbers to 'N' then switching
  $inputdate = trim($indate);
  $datesformat = str_replace($numbers, 'N', $inputdate);

  $year = FALSE; //force any non caught conditions within cases as error.
  switch ($datesformat) {
    case "NNNN[NNNN]": {
        $year = substr($inputdate, 0, 4);
        break;
      }
    case "[NNNN]":
    case "NNNN": {
        $year = $inputdate;
        break;
      }
    case "NNNN " :
    case "NNNN etc." :
    case "NNNN." : {
        $parsed = date_parse($inputdate);
        if ($parsed['year']) {
          $year = $parsed['year'];
        }
        break;
      }
    case "(NNNN) NNNN" :
    case "(NNNN)NNNN" : {
        $rightbracket = strpos($inputdate, ')');
        $midstring = substr($inputdate, $rightbracket);
        $parsed = date_parse($midstring);
        if ($parsed['year']) {
          $year = $parsed['year'];
        }
        else {
          $why = 1;
        }
        break;
      }
    case "[NNNN]NNNN" : {
        $rightbracket = strpos($inputdate, ']');
        $midstring = substr($inputdate, $rightbracket);
        $parsed = date_parse($midstring);
        if ($parsed['year']) {
          $year = $parsed['year'];
        }
        else {
          $why = 1;
        }
        break;
      }
    case "NNNN (NNNN)" :
    case "NNNN(?NNNN)" :
    case "NNNN(NNNN)" :
    case "NNNN(NNNN/N)" :
    case "NNNN(NNNN/NN)" :
    case "NNNN(NNNN/NNNN)" :
    case "NNNN(NNNN\}" :
    case "NNNN(NNNNN)" :
    case "NNNN(NNNN-NNNN)" :
    case "NNNN-NNNN(NNNN)" : {
        $leftbracket = strpos($inputdate, '(');
        $midstring = substr($inputdate, 0, $leftbracket);
        $parsed = date_parse($midstring);
        if ($parsed['year']) {
          $year = $parsed['year'];
        }
        else {
          $year = (string) $midstring;
        }
        break;
      }
    case "[NNNN?]" : {
        $parsed = date_parse($inputdate);
        if ($parsed['year']) {
          $year = '[' . $parsed['year'] . ']';
        }
        else {
          $leftbracket = strpos($inputdate, '(');
          $rightbracket = strpos($inputdate, ')');
          $midstring = substr($inputdate, $leftbracket, $rightbracket - 1);
          $parsed = date_parse($midstring);
          if ($parsed['year']) {
            $year = '[' . $parsed['year'] . ']';
          }
          else {
            $why = 1;
          }
        }
        break;
      }
    case "(?NNNN)" :
    case "(cNNNN)" :
    case "(NNNN)" :
    case "(NNNN?)" : {
        $midstring = str_replace('c', '', $inputdate);
        $midstring = str_replace('?', '', $midstring);
        $parsed = date_parse($midstring);
        if ($parsed['year']) {
          $year = '[' . $parsed['year'] . ']';
        }
        else {
          $leftbracket = strpos($midstring, '(');
          $rightbracket = strpos($midstring, ')');
          $midstring2 = substr($midstring, $leftbracket + 1, $rightbracket - 1);
          $parsed = date_parse($midstring2);
          if ($parsed['year']) {
            $year = '[' . $parsed['year'] . ']';
          }
          else {
            $year = '[' . $midstring2 . ']';
          }
        }
        break;
      }
    case "?NNNN" :
    case "c.NNNN" :
    case "cNNNN" :
    case "NNNN? " :
    case "NNNN?" : {

        $midstring = str_replace('c.', '', $inputdate);
        $midstring = str_replace('c', '', $midstring);
        $parsed = date_parse($midstring);
        if ($parsed['year']) {
          $year = '[' . $parsed['year'] . ']';
        }
        else {
          $year = '[' . str_replace('?', '', $midstring) . ']'; //handle ?2006 2004?
        }
        break;
      }
    case "n.d. " : {
        $year = 'n.d.';
        break;
      }
    case "n.d." :
    case "no date" :
    case "[n.d.]" : {
        $year = 'n.d.';
        break;
      }
    case "n.d. [NNNN]" :
    case "n.d.[approxNNNN]" :
    case "n.d.[NNNN?]" :
    case "n.d.[NNNN]" :
    case "n.D[NNNN]" :
    case "n.D. [NNNN]" :
    case "N.D. [NNNN]" : {
        $leftbracket = strpos($inputdate, '[');
        $rightbracket = strpos($inputdate, ']');
        $midstring = substr($inputdate, $leftbracket + 1, $rightbracket - ($leftbracket + 1));
        $parsed = date_parse($midstring);
        if ($parsed['year']) {
          $year = trim('[' . $parsed['year'] . ']');
        }
        else {
          $year = trim('[' . $midstring . ']');
        }
        break;
      }
    case "(NNN-)" :
    case "(NNN?)" :
    case "(NNN-?)" :
    case "[NNN-]" : {
        $parsed = date_parse($inputdate);
        if ($parsed['year']) {
          $year = '[' . $parsed['year'] . '0]';
        }
        else {
          //handle (194-) [198-]
          $year = '[' . substr($inputdate, 1, 3) . '0]';
        }
        break;
      }
    case "NNN-" :
    case "NNN? " :
    case "NNN?" :
    case "NNN-?" : {
        $parsed = date_parse($inputdate);
        if ($parsed['year']) {
          $year = '[' . $parsed['year'] . '0]';
        }
        else {
          //handle 199- 198-? 198? 
          $year = '[' . substr($inputdate, 0, 3) . '0]';
        }
        break;
      }
    case "NNN-(NNN-)" : {
        $leftbracket = strpos($inputdate, '(');
        $midstring = substr($inputdate, 0, $leftbracket);
        $parsed = date_parse($midstring);
        if ($parsed['year']) {
          $year = '[' . $parsed['year'] . '0]';
        }
        else {
          //handle 197-
          $dashpos = strpos($midstring, '-');
          $dashstring = substr($inputdate, 0, $dashpos);
          $parsed = date_parse($dashstring);
          if ($parsed['year']) {
            $year = '[' . $parsed['year'] . '0]';
          }
          else {
            $year = '[' . $dashstring . '0]';
          }
        }
        break;
      }
    case "lNNN" : {
        $parsed = date_parse($inputdate);
        if ($parsed['year']) {
          $year = '1' . $parsed['year'];
        }
        else {
          //handle l983 as date parse doesn't work
          $limapos = strpos($inputdate, 'l');
          $midstring = substr($inputdate, $limapos + 1);
          $parsed = date_parse($midstring);
          if ($parsed['year']) {
            $year = '1' . $parsed['year'];
          }
          else {
            $year = '1' . $midstring;
          }
        }
        break;
      }
    case "lNNN(NNNN)" : {
        $leftbracket = strpos($inputdate, '(');
        $midstring = substr($inputdate, 0, $leftbracket);
        $parsed = date_parse($midstring);
        if ($parsed['year']) {
          $year = '1' . $parsed['year'];
        }
        else {
          //handle l986
          $limapos = strpos($midstring, 'l');
          $limastring = substr($midstring, $limapos + 1);
          $parsed = date_parse($limastring);
          if ($parsed['year']) {
            $year = '1' . $parsed['year'];
          }
          else {
            $year = '1' . $limastring;
          }
        }
        break;
      }
    case "NNNN/N" :
    case "NNNN/N? " :
    case "NNNN/NN" :
    case "NNNN/NNNN" : {
        $slashpos = strpos($inputdate, '/');
        $midstring = substr($inputdate, 0, $slashpos);
        $parsed = date_parse($midstring);
        if ($parsed['year']) {
          $year = $parsed['year'];
        }
        else {
          $year = (string) $midstring;
        }
        break;
      }
    case "NNNN-N" :
    case "NNNN-NN" : {
        $dashpos = strpos($inputdate, '-');
        $midstring = substr($inputdate, 0, $dashpos);
        $parsed = date_parse($midstring);
        if ($parsed['year']) {
          $year = $parsed['year'];
        }
        else {
          $year = (string) $midstring;
        }
        break;
      }
    case "January NNNN" :
    case "February NNNN" :
    case "March NNNN" :
    case "April NNNN" :
    case "May NNNN" :
    case "June NNNN" :
    case "July NNNN" :
    case "August NNNN" :
    case "September NNNN" :
    case "October NNNN" :
    case "November NNNN" :
    case "December NNNN" : {
        $parsed = date_parse($inputdate);
        if ($parsed['year']) {
          $year = $parsed['year'];
        }
        else {
          $why = 1;
        }
        break;
      }
    case "NNNN-NNNN" : {
        $firstyear = intval(substr($inputdate, 0, 4));
        $secondyear = intval(substr($inputdate, 5, 4));

        if (($secondyear - $firstyear) > 2) {
          $year = $firstyear;
          $notefield = 'Published between ' . $inputdate;
        }
        else {
          $year = $secondyear;
        }
        break;
      }
    default : {
        //report as error
        $year = FALSE;
        break;
      }
  }
  return $year;
}

function getFedoraItem($pid) {
  try {
    $item = new Fedora_Item($pid);
    return $item;
  } catch (Exception $e) {
    $errormessage = $e->getMessage();
    return NULL;
  }
}