TOPDIR=$(shell pwd)

include $(TOPDIR)/common.mk

SUBDIRS:=i3-dump-log

ALL_TARGETS =
INSTALL_TARGETS =
CLEAN_TARGETS =
DISTCLEAN_TARGETS =

all: real-all

include libi3/libi3.mk
include src/i3.mk
include i3-config-wizard/i3-config-wizard.mk
include i3-msg/i3-msg.mk
include i3-input/i3-input.mk
include i3-nagbar/i3-nagbar.mk
include i3bar/i3bar.mk

real-all: $(ALL_TARGETS) subdirs

subdirs:
	for dir in $(SUBDIRS); do \
		echo ""; \
		echo "MAKE $$dir"; \
		$(MAKE) -C $$dir; \
	done

install: $(INSTALL_TARGETS)
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir install; \
	done

dist: distclean
	[ ! -d i3-${VERSION} ] || rm -rf i3-${VERSION}
	[ ! -e i3-${VERSION}.tar.bz2 ] || rm i3-${VERSION}.tar.bz2
	mkdir i3-${VERSION}
	cp i3-migrate-config-to-v4 generate-command-parser.pl i3-sensible-* i3.config.keycodes DEPENDS LICENSE PACKAGE-MAINTAINER RELEASE-NOTES-${VERSION} i3.config i3.xsession.desktop i3.applications.desktop pseudo-doc.doxygen Makefile i3-${VERSION}
	cp -r src libi3 i3-msg i3-nagbar i3-config-wizard i3bar i3-dump-log yajl-fallback include man parser-specs i3-${VERSION}
	# Only copy toplevel documentation (important stuff)
	mkdir i3-${VERSION}/docs
	# Pre-generate documentation
	$(MAKE) -C docs
	$(MAKE) -C i3bar/doc
	# Cleanup τεχ output files
	find docs -regex ".*\.\(aux\|out\|log\|toc\|bm\|dvi\|log\)" -exec rm '{}' \;
	find docs -maxdepth 1 -type f ! \( -name "*.xcf" -or -name "*.svg" \) -exec cp '{}' i3-${VERSION}/docs \;
	# Only copy source code from i3-input
	mkdir i3-${VERSION}/i3-input
	find i3-input -maxdepth 1 -type f \( -name "*.c" -or -name "*.h" -or -name "Makefile" \) -exec cp '{}' i3-${VERSION}/i3-input \;
	sed -e 's/^GIT_VERSION:=\(.*\)/GIT_VERSION:=$(shell /bin/echo '${GIT_VERSION}' | sed 's/\\/\\\\/g')/g;s/^VERSION:=\(.*\)/VERSION:=${VERSION}/g' common.mk > i3-${VERSION}/common.mk
	# Pre-generate a manpage to allow distributors to skip this step and save some dependencies
	$(MAKE) -C man
	cp man/*.1 i3-${VERSION}/man/
	cp i3bar/doc/*.1 i3-${VERSION}/i3bar/doc/
	tar cfj i3-${VERSION}.tar.bz2 i3-${VERSION}
	rm -rf i3-${VERSION}

clean: $(CLEAN_TARGETS)
	(which lcov >/dev/null 2>&1 && lcov -d . --zerocounters) || true
	$(MAKE) -C docs clean
	$(MAKE) -C man clean
	for dir in $(SUBDIRS); do \
		echo ""; \
		echo "CLEAN $$dir"; \
		$(MAKE) TOPDIR=$(TOPDIR) -C $$dir distclean; \
	done

distclean: clean $(DISTCLEAN_TARGETS)
	rm -f i3
	for dir in $(SUBDIRS); do \
		echo ""; \
		echo "DISTCLEAN $$dir"; \
		$(MAKE) TOPDIR=$(TOPDIR) -C $$dir distclean; \
	done

coverage:
	rm -f /tmp/i3-coverage.info
	rm -rf /tmp/i3-coverage
	lcov -d . -b . --capture -o /tmp/i3-coverage.info
	genhtml -o /tmp/i3-coverage/ /tmp/i3-coverage.info
