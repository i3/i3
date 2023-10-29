#!/bin/sh

set -e

for fn in distbuild/deb/debian-amd64/*.deb
do
    echo "pushing $fn to balto"
    curl \
	--header "Authorization: Bearer ${BALTO_TOKEN}" \
	--form "package=@${fn}" \
	--form distribution=all \
	https://i3.baltorepo.com/i3/i3-autobuild/upload/
done

for fn in distbuild/deb/ubuntu-amd64/*.deb
do
    echo "pushing $fn to balto"
    curl \
	--header "Authorization: Bearer ${BALTO_TOKEN}" \
	--form "package=@${fn}" \
	--form distribution=all \
	https://i3.baltorepo.com/i3/i3-autobuild-ubuntu/upload/
done
