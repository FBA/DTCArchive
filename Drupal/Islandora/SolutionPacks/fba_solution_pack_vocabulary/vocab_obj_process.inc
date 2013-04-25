<?php

/**
 * Display Vocab Object Streams
 */
class FBAVocabSolutionPack {

  private $pid = NULL;

  /**
   * Constructor
   * @param type $pid 
   */
  function __construct($pid) {
    $this->pid = $pid;
  }

  /**
   * Show MADS
   * @return type 
   */
  function showFormattedObjectDetail() {
    module_load_include('inc', 'fedora_repository', 'api/fedora_item');
    $item = new fedora_item($this->pid);
    $MADSDom = DOMDocument::loadXML($item->get_datastream_dissemination('MADS'));

    if ($MADSDom != NULL) {
      $authority = trim($MADSDom->getElementsByTagName('authority')->item(0)->nodeValue);
      $variant = trim($MADSDom->getElementsByTagName('variant')->item(0)->nodeValue);

      $notes = $MADSDom->getElementsByTagName('note');
      foreach ($notes as $node) {
        if ($node->hasAttribute('type')) {
          $typeatt = $node->getAttribute('type');
          $noteval = $node->nodeValue;

          switch ($typeatt) {
            case 'scope': {
                $scope = $noteval;
                break;
              }
            case 'definition': {
                $definition = $noteval;
                break;
              }
            case 'definition source': {
                $urlofdefsource = $noteval;
                break;
              }
            case 'approved date': {
                $dateapproved = $noteval;
                break;
              }
            default: {
                // could be "validity"
                break;
              }
          }
        }
      }

      $relateds = $MADSDom->getElementsByTagName('related');
      foreach ($relateds as $node) {
        $relval = trim($node->nodeValue);
        if (!empty($relval)) { //no point in processing null data
          if ($node->hasAttribute('type')) {
            $typeatt = $node->getAttribute('type');

            switch ($typeatt) {
              case 'broader': {
                  $broaderterms[] = $relval;

                  break;
                }
              case 'narrower': {
                  $narrowerterms[] = $relval;
                  break;
                }
              case 'other': {
                  if ($node->hasAttribute('otherType')) {
                    $othertypeatt = $node->getAttribute('otherType');

                    switch ($othertypeatt) {
                      case 'used for': {
                          $usedforterms[] = $relval;
                          break;
                        }
                      case 'use': {
                          $useinsteadterms[] = $relval;
                          break;
                        }
                      case 'preferred unit': {
                          $preferredunits = $relval;
                          break;
                        }
                    }
                  }
                  break;
                }
            }
          }
          else {
            //no type attribute therefore assumption = "related term"
            $relatedterms[] = $relval;
          }
        }
      }

      //We don't do anything with recordCreationDate/recordChangeDate at the moment
      //but here's the code just in case
//      $recordInfo = $MADSDom->getElementsByTagName('recordInfo')->item(0);
//      if ($recordInfo->hasChildNodes()) {
//        foreach ($recordInfo->childNodes as $p) {
//          $nname = $p->nodeName;
      //nname might have "mads:" prefix on it. If so, remove.
//          $pos = strpos($nname, 'mads:');
//          if ($pos === false) {
      //thats ok, mads: wasn't a prefix
//          }
//          else {
//            $nname = substr($nname, 5); //5 is length of 'mads:'
//          }
//          switch ($nname) {
//            case 'recordCreationDate': {
//                break;
//              }
//            case 'recordChangeDate': {
//                break;
//              }
//            default: {
//                break;
//              }
//          }
//        }
//      }
//      else {
      //placeholder
//      }
      // End of paragraph
    }

    $output = array(); //this will be the formatted html to be sent to the screen
    //each array entry ends up as new <tr>

    if ($authority) {
      // The custom section for a preferred term

      $output[] = array('<h1>' . $authority . '</h1>');
      //Definition
      $formattedhtml = '<p><strong>Definition</strong>';
      if ($urlofdefsource) {
        //URL of Definition Source
        $formattedhtml .= '&nbsp;(<a href="' . $urlofdefsource . '" target="_blank">' . $urlofdefsource . '</a>)';
      }
      $formattedhtml .= '</p>';
      $formattedhtml .= '<p>';
      if ($definition) {
        $formattedhtml .= $definition;
      }
      else {
        $formattedhtml .= 'N/A';
      }
      $formattedhtml .= '</p>';
      $formattedhtml .= '<p><strong>Preferred Units:</strong>&nbsp;';
      if ($preferredunits) {
        $formattedhtml .= _get_html_string_of_object_urls($preferredunits);
      }
      else {
        $formattedhtml .= 'N/A';
      }
      $formattedhtml .= '</p>';

      $formattedhtml .= '<p><strong>Scope Note:</strong><br/>';
      if ($scope) {
        $formattedhtml .= $scope;
      }
      else {
        $formattedhtml .= 'N/A';
      }
      $formattedhtml .= '</p>';

      $formattedhtml .= '<table><tr><th>Broader Terms</th><th>Narrower Terms</th><th>Related Terms</th></tr>';
      $formattedhtml .= '<tr valign="top">';
      $formattedhtml .= '<td width="33%">';
      if ($broaderterms) {
        $formattedhtml .= _get_html_string_of_object_urls($broaderterms);
      }
      else {
        $formattedhtml .= 'N/A';
      }
      $formattedhtml .= '</td>';

      $formattedhtml .= '<td width="33%">';
      if ($narrowerterms) {
        $formattedhtml .= _get_html_string_of_object_urls($narrowerterms);
      }
      else {
        $formattedhtml .= 'N/A';
      }
      $formattedhtml .= '</td>';

      $formattedhtml .= '<td width="33%">';
      if ($relatedterms) {
        $formattedhtml .= _get_html_string_of_object_urls($relatedterms);
      }
      else {
        $formattedhtml .= 'N/A';
      }
      $formattedhtml .= '</td>';
      $formattedhtml .= '</tr>';
      $formattedhtml .= '</table>';

      $formattedhtml .= '<p><strong>This term is used for the following terms:</strong>&nbsp;<br/>';
      if ($usedforterms) {
        $formattedhtml .= _get_html_string_of_object_urls($usedforterms);
      }
      else {
        $formattedhtml .= 'N/A';
      }
      $formattedhtml .= '</p>';
    }
    else {
      // The custom section for a non-preferred term
      $output[] = array('<h1>' . $variant . '</h1>');
      // Use Instead
      $formattedhtml .= '<p><strong><span style="color: red;">Use this term instead:</span></strong>&nbsp;' . _get_html_string_of_object_urls($useinsteadterms) . '</p>';
    }
    //Common footer section for both types of terms
    $formattedhtml .= '<p><strong>Approved Date</strong><br/>' . $dateapproved . '</p>';

    $formattedhtml .= '<p><a href="mailto:dis@fba.org.uk?Subject=DTC%20Vocab%20enquiry%20-%20';
    if ($authority) {
      $formattedhtml .= rawurlencode($authority);
    }
    else {
      $formattedhtml .= rawurlencode($variant);
    }
    $formattedhtml .= ' ">Click here to give feedback on this term or suggest an update.</a></p>';
    $output[] = array($formattedhtml);
    $dataStreamBody = theme('table', $headers, $output);

    $collection_fieldset = array(
      '#collapsible' => FALSE,
      '#value' => $dataStreamBody,
    );
    $tabset = array();
    $tabset['vocabtermdetail'] = array(
      '#type' => 'tabpage',
      '#title' => 'Vocabulary Term Details',
      '#content' => theme('fieldset', $collection_fieldset),
    );

    return $tabset;
  }

}

function _get_pid_from_authority($authority) {
  //get the pid from the authority/variant
  //TODO: assumption:only 1 pid for authority/variant...check this?
  $solrsearchterm = 'mads.authority:"' . trim($authority) . '" OR mads.variant:"' . trim($authority) . '"';
  $solrresult = _perform_solr_search($solrsearchterm);
  $rawresponse = $solrresult->getRawResponse();
  $resarray = json_decode($rawresponse, TRUE);
  $resdocs = $resarray['response']['docs'];
  $foreignpid = $resdocs[0][PID];
  return $foreignpid;
}

function _get_html_string_of_object_urls($terms) {
  global $base_url;
  $tempstring = '';
  if (is_array($terms)) {
    foreach ($terms as $term) {
      $termpid = _get_pid_from_authority($term);
      // A quick catch, so that an invalid url isn't put out as a link
      if ($termpid != NULL) {
        $tempstring.='<a href="' . $base_url . '/fedora/repository/' . $termpid . '">' . $term . '</a>' . '<br/>';
      }
      else {
        $tempstring.= $term;
      }
    }
  }
  else {
    $termpid = _get_pid_from_authority($terms);
    // A quick catch, so that an invalid url isn't put out as a link
    if ($termpid != NULL) {
      $tempstring.='<a href="' . $base_url . '/fedora/repository/' . $termpid . '">' . $terms . '</a>' . '<br/>';
    }
    else {
      $tempstring.= $term;
    }
  }
  return $tempstring;
}


/**
 * Blatantly ripped from apachesolr_views_query function build.
 * NB assumes solr is on localhost:8080.
 *
 * @param string $queryparam
 *   to be passed within formatted solr query as 'q'
 *
 * @return array
 *   listing all appropriate results
 */
function _perform_solr_search($queryparam = 'dummy') {

  // TODO: needs tidying.
  // encode the queryparam?

  $params = array();
  $params['rows'] = 50;
  $params['start'] = 0;
  $params['indent'] = 'on';
  $params['q'] = $queryparam;
  $params['version'] = '2.2';

  $params['fl'] = "*";
  $params['qt'] = "standard";
  $params['wt'] = "json";
  $params['hl'] = "true";
  $params['hl.fl'] = "dc.type";

  // If we display all items without pager.
  if ($params['rows'] == 0) {
    $params['rows'] = 1000000;
  }

  // Add filters to parameters.
  if (isset($params['filters'])) {
    $params['fq'] = $params['filters'];
  }

  // Add sorting.
  if (isset($params['sort'])) {
    // Populate sorting parameters.
    foreach ($params['sort'] as $field => $order) {
      $params['sort'][] = $field . ' ' . $order;
    }
  }

  // Set query.
  $dismax = NULL;
  if (isset($params['q'])) {
    // Query.
    $query = $params['q'];

    // Dismax.
    if (isset($params['defType']) AND $params['defType'] == 'dismax') {
      $dismax = $params['defType'];
      // If no qf fields are specified in the requestHandler
      // a default list is supplied here for dismax searches.
      $params['qf'] = 'dc.title^5 dc.subject^2 dc.description^2 dc.creator^2 dc.contributor^1 dc.type';
    }
  }
  else {
    $query = '*:*';
  }

  $url = variable_get('islandora_solr_search_block_url', 'http://localhost:8080/solr');
  $pathparts = parse_url($url);

  $solr = new Apache_Solr_Service($pathparts['host'], $pathparts['port'], $pathparts['path'] . '/');
  $solr->setCreateDocuments(0);

  // This is where the query gets executed and output starts being created.
  try {
    $results = $solr->search($query, $params['start'], $params['rows'], $params);
  } catch (Exception $e) {
    drupal_set_message(t('_perform_solr_search :: ') . $e->getMessage());
  }

//  $total = (int) $results->response->numFound;
//  $start = min(1, $total);
//  $end = min(($limit + $recordstart), $total);

  return $results;
}