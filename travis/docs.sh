#!/bin/sh

set -e
set -x

# TODO: install the docs via meson, inject styles with an option

for f in $(sed -n "s/^\s*'\(docs\/.*\)',$/\1/gp" meson.build | grep -vF .)
do
	asciidoc -a linkcss -a stylesdir=https://i3wm.org/css -a scriptsdir=https://i3wm.org/js --backend=xhtml11 -f docs/asciidoc-git.conf $(dirname $f)/$(basename $f .html)
done

./docs/i3-pod2html --stylesurl=https://i3wm.org/css i3-dmenu-desktop man/i3-dmenu-desktop.html
./docs/i3-pod2html --stylesurl=https://i3wm.org/css i3-save-tree man/i3-save-tree.html
./docs/i3-pod2html --stylesurl=https://i3wm.org/css build/i3test.pm docs/lib-i3test.html
./docs/i3-pod2html --stylesurl=https://i3wm.org/css testcases/lib/i3test/Test.pm docs/lib-i3test-test.html

for file in $(sed -n "s/^\s*'\(man\/.*\).man',$/\1.man/gp" meson.build)
do
	[ -f "$file" ] && asciidoc -a linkcss -a stylesdir=https://i3wm.org/css -a scriptsdir=https://i3wm.org/js --backend=xhtml11 -f docs/asciidoc-git.conf "$file"
done

mkdir -p deb/COPY-DOCS

cp $(sed -n "s/^\s*'\(docs\/.*\)',$/\1/gp" meson.build | grep -F .) deb/COPY-DOCS/
cp $(sed -n "s/^\s*'\(man\/.*\).man',$/\1.html/gp" meson.build) deb/COPY-DOCS/
