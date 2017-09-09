#!/bin/sh

set -e
set -x

GITVERSION=$(git describe --tags)

mkdir build.i3wm.org
cp -r deb/COPY-DOCS build.i3wm.org/docs
cd build.i3wm.org
echo build.i3wm.org > CNAME
git init

git config user.name "Travis CI"
git config user.email "i3bot@i3wm.org"
git add .
git commit -m "Publish docs/static analysis for github.com/i3/i3 v${GITVERSION}"

# Hide stdout/stderr because it might contain sensitive info.
set +x
echo "git push"
git push --force --quiet "https://${GH_TOKEN}@github.com/i3/build.i3wm.org.git" master:gh-pages >/dev/null 2>&1
