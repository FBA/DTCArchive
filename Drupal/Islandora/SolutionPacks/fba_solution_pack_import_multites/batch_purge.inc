  <?php

function fba_solution_pack_import_multites_form_batch_purge(&$form_state = array()) {
  $form = array();
// Set the form encoding type.
  $form['#attributes'] = array('enctype' => "multipart/form-data");

// Add a submit button.
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Batch delete multites fedora records'),
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
function fba_solution_pack_import_multites_form_batch_purge_submit($form, &$form_state) {
  if (!$form_state['form_has_errors']) {
    $batch = array(
      'title' => t('Purge some vocab objects'),
      'operations' => array(), // Operations to complete, in order. Defined below.
      'finished' => '_purge_batch_finished', // Last function to call.
      'init_message' => t('Initialising...'),
      'progress_message' => t('Processed @current out of @total.'),
      'error_message' => t('CDSisis export file to xml validation encountered an error.'),
    );

    $batch['operations'][] = array('_purge_batch_first', array());

// Tip the first domino.
    batch_set($batch);
  }
}

function fba_solution_pack_import_multites_form_batch_purge_validate($form, &$form_state) {

}

function _purge_batch_first(&$context) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  module_load_include('inc', 'fedora_repository', 'api/fedora_collection');

  $namespace = 'geo';
  $collection_pid = $namespace . ':collection';

  $pids_to_delete = _fba_get_child_collections($collection_pid);

  foreach ($pids_to_delete as $pid_to_delete) {
    $item = new Fedora_Item($pid_to_delete);
    $item->purge();
  }

  $namespace = 'vocab';
  $collection_pid = $namespace . ':collection';

  $pids_to_delete = _fba_get_child_collections($collection_pid);

  foreach ($pids_to_delete as $pid_to_delete) {
    $item = new Fedora_Item($pid_to_delete);
    $item->purge();
  }

  return TRUE;
}

function _purge_batch_finished($success, $results, $operations) {
  $message = t('All purged.');
  drupal_set_message($message);
}
/* ripped from Collection Management.inc */

function _fba_get_child_collections($collection_pid) {

  $query = <<<EOD
  select \$object from <#ri>
    where \$object  <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> <info:fedora/$collection_pid>
EOD;

  $query = htmlentities(urlencode($query));
  $content = '';

  $url = variable_get('fedora_repository_url', 'http://localhost:8080/fedora/risearch');
  $url .= "?type=tuples&flush=TRUE&format=csv&limit=$limit&offset=$offset&lang=itql&stream=on&query=" . $query;
  $content .= do_curl($url);
  $results = explode("\n", $content);
  $lines = preg_replace('/info:fedora\/|"object"/', '', $results);
  $collection_pids = array_values(array_filter($lines));
  return $collection_pids;
}