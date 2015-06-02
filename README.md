# config-extra
Home of config-merge magic

Requirements
------------
* Works with Drush master (Drush version 8.x) and Drupal 8.0.x and later.
* Requires a three-way merge tool, such as kdiff3

Installation
------------

1. cd "$HOME/.drush"
2. composer global require "drush/config-extra"
3. drush cc drush

Resources
---------
* [Drush configuration workflows](https://github.com/pantheon-systems/drush-config-workflow) can help you get started with installation, environment setup and configuration workflow planning.
* Automated tests run by [Travis CI](https://travis-ci.org/drush-ops/config-extra).
