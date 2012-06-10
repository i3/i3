A2M:=a2x -f manpage --asciidoc-opts="-f asciidoc.conf"

all: i3.1 i3-msg.1 i3-input.1 i3-nagbar.1 i3-config-wizard.1 i3-migrate-config-to-v4.1 i3-sensible-editor.1 i3-sensible-pager.1 i3-sensible-terminal.1 i3-dump-log.1

%.1: %.man asciidoc.conf
	${A2M} $<

clean:
	for file in $$(echo i3 i3-msg i3-input i3-nagbar i3-wsbar i3-config-wizard i3-migrate-config-to-v4 i3-sensible-editor i3-sensible-pager i3-sensible-terminal i3-dump-log); \
	do \
		rm -f $${file}.1 $${file}.html $${file}.xml; \
	done

distclean: clean
	rm -f *.1
