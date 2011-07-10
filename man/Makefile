all:
	a2x -f manpage --asciidoc-opts="-f asciidoc.conf" i3.man
	a2x -f manpage --asciidoc-opts="-f asciidoc.conf" i3-msg.man
	a2x -f manpage --asciidoc-opts="-f asciidoc.conf" i3-input.man
	a2x -f manpage --asciidoc-opts="-f asciidoc.conf" i3-nagbar.man
clean:
	for file in "i3 i3-msg i3-input i3-nagbar"; \
	do \
		rm -f $${file}.1 $${file}.html $${file}.xml; \
	done
