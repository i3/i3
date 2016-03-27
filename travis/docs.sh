#!/bin/sh

set -e
set -x

make -C docs ASCIIDOC="asciidoc -a linkcss -a stylesdir=http://i3wm.org/css -a scriptsdir=http://i3wm.org/js --backend=xhtml11 -f docs/asciidoc-git.conf"
./docs/i3-pod2html i3-dmenu-desktop man/i3-dmenu-desktop.html
./docs/i3-pod2html i3-save-tree man/i3-save-tree.html
for file in $(sed 's/\.1$/.man/g' debian/i3-wm.manpages)
do
	[ -f "$file" ] && asciidoc -a linkcss -a stylesdir=http://i3wm.org/css -a scriptsdir=http://i3wm.org/js --backend=xhtml11 -f docs/asciidoc-git.conf "$file"
done

mkdir -p deb/COPY-DOCS

cp $(tr "\n" ' ' < debian/i3-wm.docs) deb/COPY-DOCS/
cp $(sed 's/\.1$/.html/g' debian/i3-wm.manpages | tr "\n" ' ') deb/COPY-DOCS/
