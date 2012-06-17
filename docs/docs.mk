DISTCLEAN_TARGETS += clean-docs

# To pass additional parameters for asciidoc
ASCIIDOC = asciidoc

ASCIIDOC_NOTOC_TARGETS = \
	docs/debugging.html \
	docs/debugging-release-version.html

ASCIIDOC_TOC_TARGETS = \
	docs/hacking-howto.html \
	docs/userguide.html \
	docs/ipc.html \
	docs/multi-monitor.html \
	docs/wsbar.html \
	docs/testsuite.html \
	docs/i3bar-protocol.html

ASCIIDOC_TARGETS = \
	$(ASCIIDOC_TOC_TARGETS) \
	$(ASCIIDOC_NOTOC_TARGETS)

ASCIIDOC_CALL = $(V_ASCIIDOC)$(ASCIIDOC) -n $(ASCIIDOC_FLAGS) -o $@ $<
ASCIIDOC_TOC_CALL = $(V_ASCIIDOC)$(ASCIIDOC) -a toc -n $(ASCIIDOC_FLAGS) -o $@ $<

docs: $(ASCIIDOC_TARGETS)

$(ASCIIDOC_TOC_TARGETS): docs/%.html: docs/%
	$(ASCIIDOC_TOC_CALL)

$(ASCIIDOC_NOTOC_TARGETS): docs/%.html: docs/%
	$(ASCIIDOC_CALL)

clean-docs:
	rm -f $(ASCIIDOC_TARGETS)
