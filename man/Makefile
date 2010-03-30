A2M:=a2x -f manpage --asciidoc-opts="-f asciidoc.conf"

all: i3.1 i3-msg.1 i3-input.1 i3-wsbar.1

%.1: %.man asciidoc.conf
	${A2M} $<

i3-wsbar.1: ../i3-wsbar
	pod2man $^ > $@

clean:
	for file in "i3 i3-msg i3-input"; \
	do \
		rm -f $${file}.1 $${file}.html $${file}.xml; \
	done

distclean: clean
	rm -f *.1
