#!/usr/bin/env sh

# This script will run phpunit-based test classes using Drush's
# test framework.  First, the Drush executable is located, and
# then phpunit is invoked, passing in drush_testcase.inc as
# the bootstrap file.
#
# Any parameters that may be passed to phpunit may also be used
# with runtests.sh.

# We use the Drush that is on the global $PATH. See .travis.yml
DRUSH_PATH="$(which drush)"
SYM="`readlink "$DRUSH_PATH"`"
SYM_DIRNAME="`dirname -- "$SYM"`"
DRUSH_DIR="`cd "$(dirname -- "$DRUSH_PATH")" && cd "$SYM_DIRNAME" && pwd`/"

# Prefer the phpunit that Drush bundles, if available
PHPUNIT=phpunit
for p in "$DRUSH_DIR/vendor/bin/phpunit" "$(dirname -- "$DRUSH_PATH")"/phpunit; do
  if [ -f "$p" ]
  then
    PHPUNIT="$p"
  fi
done

# Prefer testing with Drupal 8 (maybe Drupal 7 will work someday, but not today)
if [ -z "$UNISH_DRUPAL_MAJOR_VERSION" ]
then
  export UNISH_DRUPAL_MAJOR_VERSION=8
fi

# Run all tests if there were no arguments
if [ $# = 0 ] ; then
  UNISH_NO_TIMEOUTS=y $PHPUNIT --bootstrap="$DRUSH_DIR/tests/bootstrap.inc" tests
else
  UNISH_NO_TIMEOUTS=y $PHPUNIT --bootstrap="$DRUSH_DIR/tests/bootstrap.inc" "$@"
fi
