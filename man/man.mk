DISTCLEAN_TARGETS += clean-mans

A2X = a2x

A2X_MAN_CALL = $(V_A2X)$(A2X) -f manpage --asciidoc-opts="-f man/asciidoc.conf" $(A2X_FLAGS) $<

MANS_1 = \
	man/i3.1 \
	man/i3bar.1 \
	man/i3-msg.1 \
	man/i3-input.1 \
	man/i3-nagbar.1 \
	man/i3-config-wizard.1 \
	man/i3-migrate-config-to-v4.1 \
	man/i3-sensible-editor.1 \
	man/i3-sensible-pager.1 \
	man/i3-sensible-terminal.1 \
	man/i3-dump-log.1

MANS = \
	$(MANS_1)

mans: $(MANS)

$(MANS_1): %.1: %.man man/asciidoc.conf
	$(A2X_MAN_CALL)

clean-mans:
	for file in $(notdir $(MANS)); \
	do \
		rm -f man/$${file} man/$${file%.*}.html man/$${file%.*}.xml; \
	done
