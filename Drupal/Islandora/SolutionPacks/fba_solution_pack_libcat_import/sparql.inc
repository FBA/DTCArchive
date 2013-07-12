<?php

function _libcat_get_all_mfns() {

  module_load_include('inc', 'fedora_repository', 'ObjectHelper');

  $sparql_query_string = 'select ?mfn ?object from <#ri>
where {?object <http://purl.org/dc/elements/1.1/identifier> ?mfn
FILTER regex(?mfn, "mfn:") }';

  $results = ObjectHelper::performSparqlQuery($sparql_query_string);

  $keypairresults = array();
  foreach ($results as $result) {
    $mfn = utf8_decode($result['mfn']);
    $mfn = substr($mfn,6);
    $object = (string) $result['object'];
    
    $keypairresults[$mfn] = $object;
    
  }
  return $results;
}

function _libcat_get_specific_mfn($mfntofind) {
  module_load_include('inc', 'fedora_repository', 'ObjectHelper');

  $sparql_query_string = 'select ?object ?mfn from <#ri>
  where {?object <http://purl.org/dc/elements/1.1/identifier> ?mfn
  FILTER regex(?mfn, "mfn:.' . $mfntofind . '$","i") }'; 
  //the full stop after mfn: means any character (space in this case), and the $ means end of string

  $results = ObjectHelper::performSparqlQuery($sparql_query_string);
  return $results;
}

