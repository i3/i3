#!/bin/sh

set -e
set -x

sed -i "s,%version%,$(git describe --tags),g" travis/bintray-autobuild-*.json
