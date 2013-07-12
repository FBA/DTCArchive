<?php

function _remove_fullstop($inputstring) {
//$teststring = rtrim($inputstring, '.  ');
  
  $pos = strpos($inputstring, '.');

  if ($pos) {
    $outputstring = substr($inputstring, 0, $pos);
  }
  else {
    $outputstring = $inputstring;
  }
  return $outputstring;
}
