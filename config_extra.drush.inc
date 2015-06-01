<?php

/**
 * Implements hook_drush_command().
 */
function config_extra_drush_command() {
  $items = array();

  $items['config-extra'] = array(
    'description' => "Drush command config-extra.",
    // Describe the arguments for this command.  Delete
    // this seciton if command takes no arguments.
    'arguments' => array(
      'arg' => 'Description of command argument.',
    ),
    // List all options used for this command. Only options that
    // are listed here (or that are global Drush options) may
    // be specified on the commnadline; all others raise an error.
    'options' => array(
      'option-name' => array(
        'description' => 'Description of option.',
        'example-value' => 'Example values for option, if required.',
      ),
    ),
    // Give one or more example commandline usages for this command
    'examples' => array(
      'drush config-extra --option-name arg' => 'Do something.',
    ),
    // Delete the 'outputformat' record if command does not
    // produce any output.
    'outputformat' => array(
      'default' => 'table',
      'pipe-format' => 'var_export',
      'field-labels' => array(
        'lang' => 'Language',
        'msg' => 'Message',
      ),
      'fields-default' => array('lang', 'msg'),
      'output-data-type' => 'format-table',
    ),
    'aliases' => array('shortcut'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH, // No bootstrap at all. Remove for full bootstrap of Drupal site.
  );

  // Commandfiles may also add topics.  These will appear in
  // the list of topics when `drush topic` is executed.
  $topic_file = dirname(__FILE__) . '/config-extra-topic.txt';
  if (file_exists($topic_file)) {
    $items['docs-config-extra'] = array(
      'description' => 'Description of command topic.',
      'hidden' => TRUE,
      'topic' => TRUE,
      'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
      'callback' => 'drush_print_file',
      'callback arguments' => array($topic_file),
    );
  }

  return $items;
}

/**
 * Implements hook_drush_help().
 *
 * @param
 *   A string with the help section (prepend with 'drush:')
 *
 * @return
 *   A string with the help text for your command.
 */
function config_extra_drush_help($section) {
  switch ($section) {
    case 'drush:config-extra':
      return dt("Brief help for Drush command config-extra.");
    // The 'title' meta item is used to name a group of
    // commands in `drush help`.  If a title is not defined,
    // the default is "All commands in ___", with the
    // specific name of the commandfile (e.g. config_extra).
    // Command files with less than four commands will
    // be placed in the "Other commands" section, _unless_
    // they define a title.  It is therefore preferable
    // to not define a title unless the file defines a lot
    // of commands.
    case 'meta:config_extra:title':
      return dt("config_extra commands");
    // The 'summary' meta item is displayed in `drush help --filter`,
    // and is used to give a general idea what the commands in this
    // command file do, and what they have in common.
    case 'meta:config_extra:summary':
      return dt("Summary of all commands in this command group.");
  }
}

/**
 * Implementation of drush_hook_COMMAND().
 *
 * Main command implementation goes here.
 */
function drush_config_extra() {
  // Whatever data structure is returned here will be available
  // as the 'object' item of function result when this command is
  // executed via drush_invoke_process().  If the function result
  // is a string, or if the command declares output formats,
  // then the function result will also be formatted and printed.
  return array(
    array('lang' => 'English', 'msg' => "Hello world!", ),
    array('lang' => 'Czech', 'msg' => "Ahoj, světe!", ),
    array('lang' => 'Klingon', 'msg' => "Qo' vIvan!", ),
  );
}

/**
 * Implementation of drush_hook_COMMAND_pre_validate().
 */
function drush_config_extra_pre_validate() {
  return TRUE;
}

/**
 * Implementation of drush_hook_COMMAND_validate().
 */
function drush_config_extra_validate() {
  $validated = TRUE; // replace with command validation logic
  if (!$validated) {
    return drush_set_error('DRUSH_config_extra_ERROR', dt('Command validation failed.'));
  }
  return TRUE;
}

/**
 * Implementation of drush_hook_pre_COMMAND().
 */
function drush_config_extra_pre_config_extra() {
  // Pre-command hook implementation goes here.
  return TRUE;
}

/**
 * Implementation of drush_hook_post_COMMAND().
 */
function drush_config_extra_post_config_extra() {
  // Post-command hook implementation goes here.
  return TRUE;
}

/**
 * Implementation of drush_hook_COMMAND_rollback().
 */
function drush_config_extra_rollback() {
  // Command rollback implementation goes here.
  return TRUE;
}

/**
 * Implementation of drush_hook_pm_enable_pre_validate().
 */
function drush_config_extra_pm_enable_pre_validate() {
  // If module has any required libraries, and it needs them
  // prior to enabling in order to pass validation, then we can
  // attempt to download them here.  We must look at the command
  // arguments, because PM_ENABLE_MODULES is not available
  // in pre-validate.  See drush_config_extra_post_pm_enable
  $extensions = func_get_args();
  // Deal with comma delimited extension list.
  if (strpos($extensions[0], ',') !== FALSE) {
    $extensions = explode(',', $extensions[0]);
  }
}

/**
 * Implementation of drush_hook_post_pm_enable().
 */
function drush_config_extra_post_pm_enable() {
  // If module has any required libraries, this is
  // the best place to enable them.
  /*
  $extensions = drush_get_context('PM_ENABLE_MODULES');
  if (in_array('config_extra', $extensions) && !drush_get_option('skip')) {
    drush_config_extra_download();
  }
  */
  return TRUE;
}

// Customize to download any necessary external libraries.
// Okay to delete if not needed.
function drush_config_extra_download($path = NULL) {
  if (!isset($path)) {
    if (module_exists('libraries')) {
      // Libraries 1.x will return a path even if it doesn't exist
      // while 2.x will return FALSE.
      $path = libraries_get_path('LIBRARYNAME');
      if (!$path) {
        $path = 'sites/all/libraries/LIBRARYNAME';
      }
    }
    else {
      $path = '/' . drupal_get_path('module', 'config_extra') . '/LIBRARYNAME';
    }
  }
  if (is_dir($path)) {
    drush_log('LIBRARYNAME already present. No download required.', 'ok');
  }
  else {
    $library_url = 'http://server.net/library-1.0.0.zip';
    $library_zip = drush_download_file($library_url);
    if (!$library_zip) {
      drush_log(dt('LIBRARYNAME could not be downloaded from @url', array('@url' => $library_url)), 'warning');
    }
    else {
      // Note that drush_tarball_extract will process zip files too.
      $file_list = drush_tarball_extract($library_zip, FALSE, TRUE);
      if (is_array($file_list)) {
        drush_move_dir(dirname($library_zip) . '/' . $file_list[0], $path);
        drush_log(dt('LIBRARYNAME has been extracted to @path.', array('@path' => $path)), 'success');
        return TRUE;
      }
      else {
        drush_log('LIBRARYNAME could not be extracted.', 'warning');
      }
    }
  }
  return FALSE;
}

