#!/bin/sh

set -e
set -x

DEST=$1

cd distbuild
# unpack dist tarball
mkdir -p "${DEST}"
tar xf meson-dist/*.tar.xz -C "${DEST}" --strip-components=1
cp -r ../debian "${DEST}"
sed -i '/^\s*libxcb-xrm-dev/d' deb/ubuntu-*/DIST/debian/control || true
cd "${DEST}"
debchange -m -l+g$(git describe --tags) 'Automatically built'
dpkg-buildpackage -b
