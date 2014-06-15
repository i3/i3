DISTCLEAN_TARGETS += clean-docs

# To pass additional parameters for asciidoc
ASCIIDOC = asciidoc
I3POD2HTML = ./docs/i3-pod2html

ASCIIDOC_NOTOC_TARGETS = \
	docs/debugging.html

ASCIIDOC_TOC_TARGETS = \
	docs/hacking-howto.html \
	docs/userguide.html \
	docs/ipc.html \
	docs/multi-monitor.html \
	docs/wsbar.html \
	docs/testsuite.html \
	docs/i3bar-protocol.html \
	docs/layout-saving.html

ASCIIDOC_TARGETS = \
	$(ASCIIDOC_TOC_TARGETS) \
	$(ASCIIDOC_NOTOC_TARGETS)

ASCIIDOC_CALL = $(V_ASCIIDOC)$(ASCIIDOC) -n $(ASCIIDOC_FLAGS) -o $@ $<
ASCIIDOC_TOC_CALL = $(V_ASCIIDOC)$(ASCIIDOC) -a toc -n $(ASCIIDOC_FLAGS) -o $@ $<

POD2HTML_TARGETS = \
	docs/lib-i3test.html \
	docs/lib-i3test-test.html

docs/lib-i3test.html: testcases/lib/i3test.pm
	$(V_POD2HTML)$(I3POD2HTML) $< $@

docs/lib-i3test-test.html: testcases/lib/i3test/Test.pm
	$(V_POD2HTML)$(I3POD2HTML) $< $@

docs: $(ASCIIDOC_TARGETS) $(POD2HTML_TARGETS)

$(ASCIIDOC_TOC_TARGETS): docs/%.html: docs/%
	$(ASCIIDOC_TOC_CALL)

$(ASCIIDOC_NOTOC_TARGETS): docs/%.html: docs/%
	$(ASCIIDOC_CALL)

clean-docs:
	rm -f $(ASCIIDOC_TARGETS) $(POD2HTML_TARGETS)
