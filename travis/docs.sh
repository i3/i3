#!/bin/sh

set -e
set -x

for f in $(grep '\.html$' debian/i3-wm.docs | grep -v 'docs/refcard.html' | grep -v 'docs/lib-i3test')
do
	asciidoc -a linkcss -a stylesdir=https://i3wm.org/css -a scriptsdir=https://i3wm.org/js --backend=xhtml11 -f docs/asciidoc-git.conf $(dirname $f)/$(basename $f .html)
done
./docs/i3-pod2html i3-dmenu-desktop man/i3-dmenu-desktop.html
./docs/i3-pod2html i3-save-tree man/i3-save-tree.html
./docs/i3-pod2html build/testcases/lib/i3test.pm docs/lib-i3test.html
./docs/i3-pod2html testcases/lib/i3test/Test.pm docs/lib-i3test-test.html
for file in $(sed 's/\.1$/.man/g' debian/i3-wm.manpages)
do
	[ -f "$file" ] && asciidoc -a linkcss -a stylesdir=https://i3wm.org/css -a scriptsdir=https://i3wm.org/js --backend=xhtml11 -f docs/asciidoc-git.conf "$file"
done

mkdir -p deb/COPY-DOCS

cp $(tr "\n" ' ' < debian/i3-wm.docs) deb/COPY-DOCS/
cp $(sed 's/\.1$/.html/g' debian/i3-wm.manpages | tr "\n" ' ') deb/COPY-DOCS/
