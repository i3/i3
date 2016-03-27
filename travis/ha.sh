#!/bin/sh
# Returns a hash to be used as version number suffix for the i3/travis-base
# docker container. The hash is over all files which influence what gets
# installed in the container, so that any changes in what needs to be installed
# will result in a cache invalidation.

cat debian/control "$1" | sha256sum | dd bs=1 count=8 status=none
