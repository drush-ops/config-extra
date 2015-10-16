<?php

/**
 * @file
 *   Provides Extra Configuration Management commands, notably config-merge.
 */

use Drupal\Core\Config\StorageComparer;
use Drupal\Core\Config\ConfigImporter;
use Drupal\Core\Config\FileStorage;
use Symfony\Component\Yaml\Parser;

/**
 * Implements hook_drush_command().
 */
function config_extra_drush_command() {
  $items = array();

  $items['config-merge'] = array(
    'description' => 'Merge configuration data from two sites.',
    'aliases' => array('cm'),
    'arguments' => array(
      'site' => 'Alias for the site containing the other configuration data to merge.',
      'label' => "A config directory label (i.e. a key in \$config_directories array in settings.php). Defaults to 'sync'",
    ),
    'options' => array(
      'base' => 'The commit hash or tag for the base of the three-way merge operation.  This should be the most recent commit that was deployed to the site specified in the first argument.',
      'branch' => array(
        'description' => 'A branch to use when doing the configuration merge. Optional. Default is to use a temporary branch.',
        'example-value' => 'branch-name',
      ),
      'message' => 'Commit comment for the merged configuration.',
      'no-commit' => 'Do not commit the fetched configuration; leave the modified files unstaged.',
      'tool' => array(
        'description' => 'Specific tool to use with `git mergetool`.  Use --tool=0 to prevent use of mergetool.  Optional.  Defaults to whatever tool is configured in git.',
        'example-value' => 'kdiff3',
      ),
      'fetch-only' => "Don't run `git mergetool`; fetch all configuration changes from both sites, and merge them onto the working branch.  May result in unresolved merge conflicts.",
      'git' => "Fetch changes from the other site using git instead of rsync.",
      'remote' => array(
        'description' => 'The remote git branch to use to fetch changes.  Defaults to "origin".',
        'example-value' => 'origin',
      ),
      'temp' => array(
        'description' => "Export destination site's configuration to a temporary directory.  Defaults to --temp; use --temp=0 to disable. Always ignored in --git mode.",
        'example-value' => 'path',
      ),
    ),
    'examples' => array(
      'drush @dev config-merge @production' => 'Merge configuration changes from the production site with the configuration changes made on the development site.',
      'drush @dev config-merge /path/to/drupal#sitefolder' => 'Merge configuration changes from the site indicated by the provided site specification.',
      'drush config-merge --no-commit' => 'Merge configuration changes in the database of the current site with the configuration changes in its `sync` configuration store. The merged files will remain unstaged.',
    ),
    'topics' => array('docs-cm'),
  );
  $topic_file = __DIR__ . '/docs/cm.md';
  $items['docs-cm'] = array(
    'description' => 'Configuration management on Drupal 8 with Drush.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DRUSH_BOOTSTRAP_NONE,
    'callback' => 'drush_print_file',
    'callback arguments' => array($topic_file),
  );

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
 * Implements drush config-merge command
 *
 * @param $alias
 *   The target site to merge configuration with
 * @param $config_label
 *   Which configuration set (active, etaging, etc.) to operate on
 */
function drush_config_extra_config_merge($alias = '', $config_label = 'sync') {
  // Use in log and commit messages
  $site_label = $alias;
  // If '$alias' is a 'sites' folder, then convert it into a site
  // specification, root#uri
  if (!empty($alias) && ($alias[0] != '@') && is_dir(DRUPAL_ROOT . '/sites/' . $alias)) {
    $alias = DRUPAL_ROOT . "#$alias";
  }

  // Figure out what our base commit is going to be for this operation.
  $merge_info = array(
    'base' => drush_get_option('base', FALSE),
    'message' => drush_get_option('message', ''),
    'commit' => !drush_get_option('no-commit', FALSE),
    'git-transport' => drush_get_option('git', FALSE),
    'tool' => drush_get_option('tool', ''),
    'temp' => drush_get_option('temp', TRUE),
    'config-label' => $config_label,
    'live-site' => $alias,
    'live-site-label' => $alias,
    'dev-site' => '@self',
    'remote' => drush_get_option('remote', 'origin'),
    'branch' => drush_get_option('branch', 'master'),
    'live-config' => drush_get_option('branch', FALSE),
    'dev-config' => 'drush-dev-config-temp',
    'autodelete-live-config' => FALSE,
    'autodelete-dev-config' => TRUE,
    'commit_needed' => FALSE,
    'undo-rollback' => FALSE,
  );

  // Force transport to 'git' and adjust the label
  // if there was no live site alias provided
  if (empty($merge_info['live-site'])) {
    $merge_info['git-transport'] = TRUE;
    $merge_info['live-site-label'] = 'git';
  }

  $result = _drush_cme_get_initial_vcs_state($merge_info);
  if ($result === FALSE) {
    return FALSE;
  }

  // If the user did not speicfy a branch, then fill in a default value
  if (!$merge_info['live-config']) {
    if (empty($merge_info['live-site'])) {
      $merge_info['live-config'] = $merge_info['original-branch'];
    }
    else {
      $merge_info['live-config'] = 'drush-live-config-temp';
      $merge_info['autodelete-live-config'] = TRUE;
    }
  }

  drush_log(dt("Working branch is !branch.", array('!branch' => $merge_info['live-config'])), 'debug');

  $result = _drush_cme_prepare_for_export($merge_info);
  if ($result === FALSE) {
    return FALSE;
  }
  $result = _drush_cme_export_remote_configuration_before_merge($merge_info);
  if ($result === FALSE) {
    return FALSE;
  }

  // Make sure that our local branch is ready prior to doing a 'git pull', etc.
  $result = _drush_cme_prepare_local_branch_for_configuration_transport($merge_info);
  if ($result === FALSE) {
    return FALSE;
  }

  // Copy the exported configuration from 'live-site', either via git pull or via rsync
  if ($merge_info['git-transport']) {
    $result = _drush_cme_copy_remote_configuration_via_git($merge_info);
  }
  else {
    $result = _drush_cme_copy_remote_configuration_via_rsync($merge_info);
  }
  if ($result === FALSE) {
    return FALSE;
  }

  // Exit if there were no changes from 'live-site'.
  if (empty($merge_info['changed_configuration_files'])) {
    drush_log(dt("No configuration changes on !site; nothing to do here.", array('!site' => $merge_info['live-site-label'])), 'ok');
    _drush_config_extra_merge_cleanup($merge_info);
    return TRUE;
  }

  $result = _drush_cme_commit_transmitted_configuration_changes($merge_info);
  if ($result === FALSE) {
    return FALSE;
  }

  $result = _drush_cme_prepare_for_local_configuration_export($merge_info);
  if ($result === FALSE) {
    return FALSE;
  }
  $result = _drush_cme_export_local_configuration($merge_info);
  if ($result === FALSE) {
    return FALSE;
  }

  // Check to see if the export changed any files.  If it did not, then
  // skip the merge, and process only the config pulled in from the other site.
  // TODO: This needs to be a diff against the base commit.  In 'git' mode,
  // we probably want to just skip this test and always merge.  Maybe always do this?
  $configuration_path = _drush_cme_get_configuration_path($merge_info);
  $result = drush_shell_cd_and_exec($configuration_path, 'git status --porcelain .');
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("`git status` failed."));
  }
  $changed_configuration_files = drush_shell_exec_output();
  if (empty($changed_configuration_files)) {
    drush_log(dt("No configuration changes on !site; no merge necessary.", array('!site' => $merge_info['dev-site'])), 'ok');
  }
  else {
    $result = _drush_cme_merge_local_and_remote_configurations($merge_info);
  }
  if ($result === FALSE) {
    return FALSE;
  }
  $result = _drush_cme_merge_to_original_branch($merge_info);

  return $result;
}

function _drush_cme_get_configuration_path(&$merge_info) {
  // Find the current configuration path
  if (!isset($merge_info['configuration_path'])) {
    // Get the configuration path from the local site.
    $merge_info['configuration_path'] = config_get_config_directory($merge_info['config-label']);
  }
  return $merge_info['configuration_path'];
}

function _drush_cme_get_remote_configuration_path(&$merge_info) {
  // If there is no "live" site, then use the local configuration path
  if (empty($merge_info['live-site'])) {
    return _drush_cme_get_configuration_path($merge_info);
  }
  if (!isset($merge_info['remote_configuration_path'])) {
    $configdir_values = drush_invoke_process($merge_info['live-site'], 'drupal-directory', array('config-' . $merge_info['config-label']));
    $merge_info['remote_configuration_path'] = trim($configdir_values['output']);
  }
  return $merge_info['remote_configuration_path'];
}

function _drush_cme_get_initial_vcs_state(&$merge_info) {
  $configuration_path = _drush_cme_get_configuration_path($merge_info);
  // Is the selected configuration directory under git revision control?  If not, fail.
  $result = drush_shell_cd_and_exec($configuration_path, 'git rev-parse --abbrev-ref HEAD');
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_MERGE_NO_GIT', dt("The drush config-merge command requires that the selected configuration directory !dir be under git revision control.", array('!dir' => $configuration_path)));
  }
  $output = drush_shell_exec_output();
  $original_branch = $output[0];
  drush_log(dt("Original branch is !branch", array('!branch' => $original_branch)), 'debug');
  $merge_info['original-branch'] = $original_branch;

  // Find the current sha-hash
  $result = drush_shell_cd_and_exec($configuration_path, 'git rev-parse HEAD');
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_MERGE_NO_GIT', dt("`git rev-parse HEAD` failed."));
  }
  $output = drush_shell_exec_output();
  $merge_info['original_hash'] = $output[0];

  // Fail if there are any uncommitted changes on the current branch
  // inside the configuration path.
  $result = drush_shell_cd_and_exec($configuration_path, 'git status --porcelain .');
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("`git status` failed."));
  }
  $uncommitted_changes = drush_shell_exec_output();
  if (!empty($uncommitted_changes)) {
    return drush_set_error('DRUSH_CONFIG_MERGE_UNCOMMITTED_CHANGES', dt("Working set has uncommitted changes; please commit or discard them before merging.  `git stash` before `drush config-merge`, and `git stash pop` afterwards can be useful here.\n\n!changes", array('!changes' => $uncommitted_changes)));
  }
}

function _drush_cme_prepare_for_export(&$merge_info) {
  $configuration_path = _drush_cme_get_configuration_path($merge_info);
  // The git transport only works if both sites have the same config path, so look up the
  // remote config path to see if this is the case, and error out if it is not.
  if ($merge_info['git-transport']) {
    $remote_configuration_path = _drush_cme_get_remote_configuration_path($merge_info);
    // n.b. $configuration_path is a relative path, whereas drupal-directory will give us
    // an absolute path.  We therefore compare only the ends of the strings.
    if ($configuration_path != substr(trim($remote_configuration_path), -strlen($configuration_path))) {
      return drush_set_error('CONFIG_MERGE_INCOMPATIBLE_PATHS', dt("The --git option only works when the configuration path is the same on the source and destination sites.  On your source site, the configuration path is !remote; on the target site, it was !local.  You must use the default transport mechanism (rsync).", array('!remote' => $configdir_values['output'], '!local' => $configuration_path)));
    }
  }

  // Find the merge base
  $result = drush_shell_cd_and_exec($configuration_path, 'git merge-base HEAD %s/%s', $merge_info['remote'], $merge_info['branch']);
  if (!$result) {
    // If there is no remote/branch, then we'll just use the current hash as the merge base.
    $merge_info['merge-base'] = $merge_info['original_hash'];
  }
  else {
    $output = drush_shell_exec_output();
    $merge_info['merge-base'] = $output[0];
  }

  // If the user did not supply a base commit, then we'll fill in
  // the merge base that we looked up via git.
  if (!$merge_info['base']) {
    $merge_info['base'] = $merge_info['merge-base'];
  }

  // Decide how we are going to transfer the exported configuration.
  $merge_info['export_options'] = array();

  // Check to see if the user wants to use git to transfer the configuration changes;
  // if so, set up the appropriate options to pass along to config-export.
  if ($merge_info['git-transport']) {
    $merge_info['export_options']['push'] = TRUE;
    $merge_info['export_options']['remote'] = $merge_info['remote'];
    $merge_info['export_options']['branch'] = $merge_info['live-config'];
  }
  elseif ($merge_info['temp']) {
    if ($merge_info['temp'] === TRUE) {
      $merge_info['export_options']['destination'] = '%temp/config';
    }
    else {
      $merge_info['export_options']['destination'] = $merge_info['temp'];
    }
  }

  // In rsync mode, this is where we will copy from.  (Skip this assignment if
  // someone already set up or looked up the remote path.)
  if (!isset($merge_info['remote_configuration_path'])) {
    $merge_info['remote_configuration_path'] = "%config-" . $merge_info['config-label'];
  }
  $merge_info['rsync_options'] = array('delete' => TRUE);

  // Make a temporary copy of our configuration directory, so that we
  // can record what changed after calling config-export and merging.
  $merge_info['original_configuration_files'] = drush_tempdir() . '/original';
  drush_copy_dir($configuration_path, $merge_info['original_configuration_files'], FILE_EXISTS_OVERWRITE);
}

function _drush_cme_export_remote_configuration_before_merge(&$merge_info) {
  // We can skip the export if no site alias was provided for the "live" site.
  if (!empty($merge_info['live-site'])) {
    // Run config-export on the live site.
    $values = drush_invoke_process($merge_info['live-site'], 'config-export', array($merge_info['config-label']), $merge_info['export_options']);
    if ($values['error_status']) {
      return drush_set_error('DRUSH_CONFIG_MERGE_CANNOT_EXPORT', dt("Could not export configuration for site !site", array('!site' => $merge_info['live-site'])));
    }
    // After we run config-export, we remember the path to the directory
    // where the exported configuration was written.
    if (!empty($values['object']) && ($merge_info['temp'])) {
      $merge_info['remote_configuration_path'] = $values['object'];
      // $merge_info['rsync_options']['remove-source-files'] = TRUE;
    }
  }
}

function _drush_cme_copy_remote_configuration_via_git(&$merge_info) {
  $configuration_path = _drush_cme_get_configuration_path($merge_info);
  // If the config-export command worked, and exported changes, then this should
  // pull down the appropriate commit, which should change files in $configuration_path
  // (and nowhere else).
  $result = drush_shell_cd_and_exec($configuration_path, 'git pull %s %s', $merge_info['remote'], $merge_info['branch']);
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_EXPORT_FAILURE', dt("`git pull` failed.  Output:\n\n!output", array('!output' => implode("\n", drush_shell_exec_output()))));
  }
  // Check out the 'live-config' branch.  This should always exist; config-export should create it for us.
  $result = drush_shell_cd_and_exec($configuration_path, 'git fetch');
  $result = drush_shell_cd_and_exec($configuration_path, 'git checkout %s', $merge_info['live-config']);
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("Could not switch to working branch !b", array('!b' => $merge_info['live-config'])));
  }
  // Let's check to see if anything changed in the branch we just pulled over.
  $result = drush_shell_cd_and_exec($configuration_path, 'git diff-tree --no-commit-id --name-only -r HEAD %s .', $merge_info['original_hash']);
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_EXPORT_FAILURE', dt("`git diff-tree` failed."));
  }
  $merge_info['changed_configuration_files'] = drush_shell_exec_output();
}

function _drush_cme_copy_remote_configuration_via_rsync(&$merge_info) {
  $configuration_path = _drush_cme_get_configuration_path($merge_info);
  $remote_configuration_path = _drush_cme_get_remote_configuration_path($merge_info);
  // Create a new temporary branch to hold the configuration changes
  // from the site 'live-config'.  The last parameter is the 'start point',
  // which is like checking out the specified sha-hash before creating the
  // branch.
  if ($merge_info['autodelete-live-config']) {
    $result = drush_shell_cd_and_exec($configuration_path, 'git checkout -B %s %s', $merge_info['live-config'], $merge_info['base']);
    if (!$result) {
      return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("Could not create temporary branch !b", array('!b' => $merge_info['live-config'])));
    }
  }
  else {
    $result = drush_shell_cd_and_exec($configuration_path, 'git checkout -b %s', $merge_info['live-config']);
  }
  // We set the upstream branch as a service for the user, to help with
  // cleanup should this process end before completion.  We skip this if
  // the branch already existed (i.e. with --branch option).
  if ($result) {
    drush_shell_cd_and_exec($configuration_path, 'git branch --set-upstream-to=%s', $merge_info['original-branch']);
  }
  // Copy the exported configuration files from 'live-site' via rsync and commit them
  $values = drush_invoke_process($merge_info['dev-site'], 'core-rsync', array($merge_info['live-site'] . ":$remote_configuration_path/", $merge_info['dev-site'] . ":$configuration_path/"), $merge_info['rsync_options']);
  if ($values['error_status']) {
    return drush_set_error('DRUSH_CONFIG_MERGE_RSYNC_FAILED', dt("Could not rsync from !live to !dev.", array('!live' => $merge_info['live-config'], '!dev' => $merge_info['dev-config'])));
  }

  // Commit the new changes to the branch prepared for @live. Exit with
  // "nothing to do" if there are no changes to be committed.
  $result = drush_shell_cd_and_exec($configuration_path, 'git status --porcelain .');
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("`git status` failed."));
  }
  $merge_info['changed_configuration_files'] = drush_shell_exec_output();
  $merge_info['commit_needed'] = TRUE;
}

function _drush_cme_commit_transmitted_configuration_changes(&$merge_info) {
  $configuration_path = _drush_cme_get_configuration_path($merge_info);
  // Commit the files brought over via rsync.
  if ($merge_info['commit_needed']) {
    $result = drush_shell_cd_and_exec($configuration_path, 'git add -A .');
    if (!$result) {
      return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("`git add -A` failed."));
    }
    // Commit the changes brought over from the live site.
    $result = drush_shell_cd_and_exec($configuration_path, 'git commit -m %s', 'Drush config-merge exported configuration from ' . $merge_info['live-site-label'] . ' ' . $merge_info['message']);
    if (!$result) {
      return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("`git commit` failed."));
    }
  }
}

function _drush_cme_prepare_local_branch_for_configuration_transport($merge_info) {
  $configuration_path = _drush_cme_get_configuration_path($merge_info);
  // Create a new temporary branch to hold the configuration changes
  // from the dev site ('@self').  We want to take all of the commits
  // on the original branch.
  $result = drush_shell_cd_and_exec($configuration_path, 'git checkout -B %s', $merge_info['dev-config']);
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("Could not create temporary branch !b", array('!b' => $merge_info['dev-config'])));
  }
  // We set the upstream branch as a service for the user, to help with
  // cleanup should this process end before completion.
  drush_shell_cd_and_exec($configuration_path, 'git branch --set-upstream-to=%s', $merge_info['original-branch']);
  // Return to the original branch
  drush_shell_cd_and_exec($configuration_path, 'git checkout %s', $merge_info['original-branch']);

  // Special git-fu:  if we are going to 'git pull' onto the original branch, then rewind to the merge point
  // to avoid conflicts at the point where we're doing 'git pull'.  If the merge-base == the original hash,
  // then there are no commits that have not been pushed to the central repository, so we can skip this.
  //
  // Here is a diagram showing our situation.  Both 'dev' and 'live' are
  // working off of the 'master' branch.
  //
  // Dev commits:
  //
  // A---B---C---1---2---3  master
  //
  // Commits in the central repository, from the "live" site:
  //
  // A---B---C---i---j---k  master
  //
  // After 'git checkout -B dev-config' and 'git reset --hard [merge-base]':
  //
  //           1---2---3  dev-config
  //          /
  // A---B---C  master
  //
  // And later, when we run 'git pull' to get the commits that were pushed
  // to 'master' from the "live" site, we will get this:
  //
  //           1---2---3  dev-config
  //          /
  // A---B---C---i---j---k  master
  //
  // We will then rebase 'dev-config' into 'master', and then run the
  // three-way-merge tool.
  //
  // It would cause considerable alarm to the user if we ran 'git reset --hard',
  // and then some failure caused the config-merge command to abort, as the
  // user would not know where to find their missing commits.  To avoid this,
  // we take care to keep track of the original hash from the head of master
  // (commit "3" in the diagram above) so that we can restore the starting
  // branch to its original state during the rollback function.
  //
  if (($merge_info['original-branch'] == $merge_info['live-config']) && ($merge_info['original_hash'] != $merge_info['merge-base'])) {
    drush_shell_cd_and_exec($configuration_path, 'git reset --hard %s', $merge_info['merge-base']);
    // Mark this to roll back to our original state in case of a later failure.
    $merge_info['undo-rollback'] = $merge_info['original_hash'];
  }
}

function _drush_cme_prepare_for_local_configuration_export(&$merge_info) {
  $configuration_path = _drush_cme_get_configuration_path($merge_info);
  // Create a new temporary branch to hold the configuration changes
  // from the dev site ('@self').
  $result = drush_shell_cd_and_exec($configuration_path, 'git checkout %s', $merge_info['dev-config']);
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("Could not switch to temporary branch !b", array('!b' => $merge_info['dev-config'])));
  }
}

function _drush_cme_export_local_configuration(&$merge_info) {
  // Run drush @dev cex label
  $values = drush_invoke_process($merge_info['dev-site'], 'config-export', array($merge_info['config_label']));
  if ($values['error_status']) {
    return drush_set_error('DRUSH_CONFIG_MERGE_CANNOT_EXPORT', dt("Could not export configuration for site !site", array('!site' => $merge_info['dev-site'])));
  }
}

function _drush_cme_merge_local_and_remote_configurations(&$merge_info) {
  $configuration_path = _drush_cme_get_configuration_path($merge_info);

  $result = drush_shell_cd_and_exec($configuration_path, 'git add -A .');
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("`git add -A` failed."));
  }
  // Note that this commit will be `merge --squash`-ed away.  We'll put in
  // a descriptive message to help users understand where it came from, if
  // they end up with dirty branches after an aborted run.
  $result = drush_shell_cd_and_exec($configuration_path, 'git commit -m %s', 'Drush config-merge exported configuration from ' . $merge_info['dev-site'] . ' ' . $merge_info['message']);
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("`git commit` failed."));
  }

  // git checkout live-config && git rebase dev-config.
  // This will put us back on the live-config branch,
  // merge in the changes from the temporary dev branch,
  // and rebase the live-config branch to include all of
  // the commits from the dev config branch.
  $result = drush_shell_cd_and_exec($configuration_path, 'git checkout %s && git rebase %s', $merge_info['live-config'], $merge_info['dev-config']);

  // We don't need the dev-config branch any more, so we'll get rid of
  // it right away, so there is less to clean up / hang around should
  // we happen to abort before everything is done.
  if ($merge_info['autodelete-dev-config']) {
    drush_shell_cd_and_exec($configuration_path, 'git branch -D %s 2>/dev/null', $merge_info['dev-config']);
  }

  // If there are MERGE CONFLICTS: prompt the user and run 3-way diff tool.
  $result = drush_shell_cd_and_exec($configuration_path, 'git status --porcelain .', $configuration_path);
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("`git status` failed."));
  }
  // Check to see if any line in the output starts with 'UU'.
  // This means "both sides updated" -- i.e. a conflict.
  $conflicting_configuration_changes = drush_shell_exec_output();
  $conflicting_files = array_reduce(
    $conflicting_configuration_changes,
    function($reduce, $item) use ($configuration_path) {
      if (substr($item,0,2) == "UU") {
        $reduce[] = str_replace($configuration_path . '/', '', substr($item, 3));
      }
      return $reduce;
    },
    array()
  );
  // Report on any conflicts found.
  if (!empty($conflicting_files)) {
    drush_print("\nCONFLICTS:\n");
    drush_print(implode("\n", $conflicting_files));
    drush_print("\n");
  }

  // Stop right here if the user specified --merge-only.
  if (drush_get_option('fetch-only', FALSE)) {
    drush_log(dt("Specified --fetch-only, so stopping here after the merge.  Use `git checkout !b` to return to your original branch.", array('!b' => $merge_info['original-branch'])), 'ok');
    return TRUE;
  }

  // If there are any conflicts, run the merge tool.
  if (!empty($conflicting_files)) {
    if (!$merge_info['tool'] && ($merge_info['tool'] != '')) {
      // If --tool=0, then we will never run the merge tool
      return drush_set_error('DRUSH_CONFLICTS_NOT_MERGED', dt("There were conflicts that needed merging, but mergetool disabled via --tool option.  Rolling back; run again with --fetch-only to stop prior to merge."));
    }
    $choice = 'mergetool';
    while ($choice == 'mergetool') {
      if (empty($merge_info['tool'])) {
        $result = drush_shell_cd_and_exec($configuration_path, 'git mergetool .');
      }
      else {
        $result = drush_shell_cd_and_exec($configuration_path, 'git mergetool --tool=%s .', $merge_info['tool']);
      }
      // There is no good way to tell what the result of 'git mergetool'
      // was.
      //
      // The documentation says that $result will be FALSE if the user
      // quits without saving; however, in my experience, git mergetool
      // hangs, and never returns if kdiff3 or meld exits without saving.
      //
      // We will not allow the user to continue if 'git mergetool' exits with
      // an error.  If there was no error, we will ask the user how to continue,
      // since save and exit does not necessarily mean that the user was
      // satisfied with the result of the merge.
      $done = array();
      if ($result) {
        if ($merge_info['commit']) {
          $done = array('done' => dt("All conflicts resolved!  Commit changes, re-import configuration and exit."));
        }
        else {
          $done = array('done' => dt("All conflicts resolved!  Re-import configuration and exit with unstaged changes."));
        }
      }
      $selections = $done + array(
        'abandon' => dt("Abandon merge; erase all work, and go back to original state."),
        'mergetool' => dt("Run mergetool again."),
      );
      $choice = drush_choice($selections, dt("Done with merge.  What would you like to do next?"));
      // If the user cancels, we must call drush_user_abort() for things to work right.
      if ($choice === FALSE) {
        return drush_user_abort();
      }
      // If there is an action function, then call it.
      $fn = '_drush_config_extra_merge_action_' . $choice;
      if (function_exists($fn)) {
        $choice = $fn($merge_info);
      }
      // If the action function returns TRUE or FALSE, then
      // return with that result without taking further action.
      if (is_bool($choice)) {
        return $choice;
      }
    }
    // Commit the results of the merge to the working branch.  This
    // commit will be squash-merged with the others below; if the
    // --no-commit option was selected, the results of the squash-merge
    // will remain unstaged.
    $result = drush_shell_cd_and_exec($configuration_path, 'git add -A .');
    if (!$result) {
      return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("`git add -A` failed."));
    }
    $result = drush_shell_cd_and_exec($configuration_path, 'git commit -m %s', 'Drush config-merge merge commit for ' . $merge_info['live-site-label']. ' configuration with ' . $merge_info['dev-site'] . ' configuration.');
    if (!$result) {
      return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("`git commit` failed."));
    }
  }
}

function _drush_cme_merge_to_original_branch(&$merge_info) {
  $configuration_path = _drush_cme_get_configuration_path($merge_info);

  // Merge the results of the 3-way merge back to the original branch.
  drush_shell_cd_and_exec($configuration_path, 'git checkout %s', $merge_info['original-branch']);
  // Run 'git merge' and 'git commit' as separate operations, as 'git merge --squash'
  // ignores the --commit option.  We will take 'theirs' here, because all of the commits
  // on the original branch were part of the 3-way-merge that we just completed with
  // the 'live-config' branch.
  $result = drush_shell_cd_and_exec($configuration_path, 'git merge -X theirs --no-commit %s', $merge_info['live-config']);
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("`git merge --squash` failed.  Output:\n\n!output", array('!output' => implode("\n", drush_shell_exec_output()))));
  }

  // Re-import the merged changes into the database for the local site.
  drush_set_option('strict', 0);
  $result = drush_invoke('config-import', array($merge_info['config_label']));
  if ($result === FALSE) {
    // If there was an error, or nothing to import, return FALSE,
    // signaling rollback.
    return FALSE;
  }

  // Check to see if the merge resulted in any changed files.
  // If there were no changes in dev, then there might not be
  // anything to do here.
  $result = drush_shell_cd_and_exec($configuration_path, 'git status --porcelain .');
  if (!$result) {
    return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("`git status` failed."));
  }
  $files_changed_by_merge = drush_shell_exec_output();

  // If there were any files changed in the merge, then import them and commit.
  if (!empty($files_changed_by_merge)) {
    if ($merge_info['commit']) {
      if (empty($merge_info['message'])) {
        // The 'dev-site' is probably just '@self', so we'll put the site-name
        // in the comment, which hopefully will read okay
        $config = Drupal::config('system.site');
        $site_name = $config->get('name');
        $merge_info['message'] = dt("Merged configuration from @live in @site", array('@live' => $merge_info['live-site-label'], '@site' => $site_name));

        // Retrieve a list of differences between the active and target configuration (if any).
        $target_storage = new FileStorage($configuration_path);
        /** @var \Drupal\Core\Config\StorageInterface $active_storage */
        $active_storage = new FileStorage($merge_info['original_configuration_files']);
        $config_comparer = new StorageComparer($active_storage, $target_storage, Drupal::service('config.manager'));
        if ($config_comparer->createChangelist()->hasChanges()) {
          $change_list = array();
          foreach ($config_comparer->getAllCollectionNames() as $collection) {
            $change_list[$collection] = $config_comparer->getChangelist(NULL, $collection);
          }
          $tbl = _drush_format_config_changes_table($change_list);
          $output = $tbl->getTable();
          if (!stristr(PHP_OS, 'WIN')) {
            $output = str_replace("\r\n", PHP_EOL, $output);
          }
          $merge_info['message'] .= "\n\n$output";
        }
      }
      $comment_file = drush_save_data_to_temp_file($merge_info['message']);
      $result = drush_shell_cd_and_exec($configuration_path, 'git commit --file=%s', $comment_file);
      if (!$result) {
        return drush_set_error('DRUSH_CONFIG_MERGE_FAILURE', dt("`git commit` failed."));
      }
    }
  }
  _drush_config_extra_merge_cleanup($merge_info);
  return TRUE;
}

/**
 * If drush_config_merge() exits with an error, then Drush will
 * call the rollback function, so that we can clean up.  We call
 * the cleanup function explicitly if we exit with no error.
 */
function drush_config_extra_config_merge_rollback() {
  $merge_info = drush_get_context('DRUSH_CONFIG_MERGE_INFO');
  _drush_config_extra_merge_cleanup($merge_info);

  // If we messed with the commits on the original branch, then we need to put them back
  // the way they were if we roll back.  We don't want to do this on an ordinary cleanup, though.
  if (isset($merge_info['undo-rollback'])) {
    drush_shell_cd_and_exec($configuration_path, 'git reset --hard %s', $merge_info['merge-base']);
    drush_shell_cd_and_exec($configuration_path, 'git merge %s', $merge_info['undo-rollback']);
  }
}

/**
 * If the user wants to abandon the work of their merge, then
 * clean up our temporary branches and return TRUE to cause
 * the calling function to exit without committing.
 */
function _drush_config_extra_merge_action_abandon(&$merge_info) {
  _drush_config_extra_merge_cleanup($merge_info);
  drush_log(dt("All changes erased."), 'ok');
  return TRUE;
}

/* Helper functions */

/**
 * Reset our state after a config-merge command
 */
function _drush_config_extra_merge_cleanup($merge_info) {
  if (!empty($merge_info) && !empty($merge_info['configuration_path'])) {
    $configuration_path = $merge_info['configuration_path'];
    // If we are in the middle of a rebase, we must abort, or
    // git will remember this state for a long time (that is,
    // you can switch away from this branch and come back later,
    // and you'll still be in a "rebasing" state.)
    drush_shell_cd_and_exec($configuration_path, 'git rebase --abort');
    // Violently delete any untracked files in the configuration path
    // without prompting.  This isn't as dangerous as it sounds;
    // drush config-merge refuses to run if you have untracked files
    // here, and you can get anything that Drush config-merge put here
    // via `drush cex` (or just run config-merge again).
    drush_shell_cd_and_exec($configuration_path, 'git clean -d -f .');
    // Switch back to the branch we started on.
    $result = drush_shell_cd_and_exec($configuration_path, 'git checkout %s', $merge_info['original-branch']);
    if (!$result) {
      drush_log(dt("Could not return to original branch !branch", array('!branch' => $merge_info['original-branch'])), 'warning');
    }
    // Delete our temporary branches
    if ($merge_info['autodelete-live-config']) {
      drush_shell_cd_and_exec($configuration_path, 'git branch -D %s 2>/dev/null', $merge_info['live-config']);
    }
    if ($merge_info['autodelete-dev-config']) {
      drush_shell_cd_and_exec($configuration_path, 'git branch -D %s 2>/dev/null', $merge_info['dev-config']);
    }
  }
}
