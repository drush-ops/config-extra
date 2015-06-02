#!/usr/bin/env sh

# This script will run phpunit-based test classes using Drush's
# test framework.  First, the Drush executable is located, and
# then phpunit is invoked, passing in drush_testcase.inc as
# the bootstrap file.
#
# Any parameters that may be passed to phpunit may also be used
# with runtests.sh.

DRUSH_PATH="$(which drush)"
DRUSH_DIR="$(dirname -- "$DRUSH_PATH")"

PHPUNIT=phpunit

if [ -f "$DRUSH_DIR/vendor/bin/phpunit" ]
then
  PHPUNIT="$DRUSH_DIR/vendor/bin/phpunit"
fi

echo "DRUSH_PATH is $DRUSH_PATH"
echo "PHPUNIT is $PHPUNIT"

if [ $# = 0 ] ; then
  UNISH_DRUPAL_MAJOR_VERSION=8 UNISH_NO_TIMEOUTS=y $PHPUNIT --bootstrap="$DRUSH_DIR/tests/bootstrap.inc" tests
else
  UNISH_DRUPAL_MAJOR_VERSION=8 UNISH_NO_TIMEOUTS=y $PHPUNIT --bootstrap="$DRUSH_DIR/tests/bootstrap.inc" "$@"
fi
