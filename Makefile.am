@CODE_COVERAGE_RULES@

echo-version:
	@echo "@I3_VERSION@"

bin_PROGRAMS = \
	i3 \
	i3bar/i3bar \
	i3-config-wizard/i3-config-wizard \
	i3-dump-log/i3-dump-log \
	i3-input/i3-input \
	i3-msg/i3-msg \
	i3-nagbar/i3-nagbar

install-exec-hook:
	$(LN_S) -f i3 $(DESTDIR)$(bindir)/i3-with-shmlog

uninstall-hook:
	rm -f $(DESTDIR)$(bindir)/i3-with-shmlog

i3includedir=$(includedir)/i3
i3include_HEADERS = \
	include/i3/ipc.h

dist_bin_SCRIPTS = \
	i3-dmenu-desktop \
	i3-migrate-config-to-v4 \
	i3-save-tree \
	i3-sensible-editor \
	i3-sensible-pager \
	i3-sensible-terminal

i3confdir = $(sysconfdir)/i3
dist_i3conf_DATA = \
	etc/config \
	etc/config.keycodes

applicationsdir = $(datarootdir)/applications
xsessionsdir = $(datarootdir)/xsessions
dist_applications_DATA = \
	share/applications/i3.desktop
dist_xsessions_DATA = \
	share/xsessions/i3.desktop \
	share/xsessions/i3-with-shmlog.desktop

noinst_LIBRARIES = libi3.a

check_PROGRAMS = \
	test.commands_parser \
	test.config_parser \
	test.inject_randr15

check_SCRIPTS = \
	testcases/complete-run.pl

check_DATA = \
	anyevent-i3.stamp

clean-check:
	rm -rf testsuite-* latest i3-cfg-for-* _Inline
clean-local: clean-check

TESTS = testcases/complete-run.pl

EXTRA_DIST = \
	$(dist_docs_toc_DATA:.html=) \
	$(dist_docs_notoc_DATA:.html=) \
	AnyEvent-I3/Changes \
	AnyEvent-I3/MANIFEST \
	AnyEvent-I3/MANIFEST.SKIP \
	AnyEvent-I3/Makefile.PL \
	AnyEvent-I3/README \
	AnyEvent-I3/lib/AnyEvent/I3.pm \
	AnyEvent-I3/t/00-load.t \
	AnyEvent-I3/t/01-workspaces.t \
	AnyEvent-I3/t/02-sugar.t \
	AnyEvent-I3/t/boilerplate.t \
	AnyEvent-I3/t/manifest.t \
	AnyEvent-I3/t/pod-coverage.t \
	AnyEvent-I3/t/pod.t \
	contrib/dump-asy.pl \
	contrib/gtk-tree-watch.pl \
	contrib/i3-wsbar \
	contrib/per-workspace-layout.pl \
	contrib/trivial-bar-script.sh \
	docs/asciidoc-git.conf \
	docs/bigpicture.png \
	docs/i3-pod2html \
	docs/i3-sync.dia \
	docs/i3-sync.png \
	docs/i3-sync-working.dia \
	docs/i3-sync-working.png \
	docs/keyboard-layer1.png \
	docs/keyboard-layer2.png \
	docs/layout-saving-1.png \
	docs/logo-30.png \
	docs/modes.png \
	docs/refcard.html \
	docs/refcard_style.css \
	docs/single_terminal.png \
	docs/snapping.png \
	docs/tree-layout1.png \
	docs/tree-layout2.png \
	docs/tree-shot1.png \
	docs/tree-shot2.png \
	docs/tree-shot3.png \
	docs/tree-shot4.png \
	docs/two_columns.png \
	docs/two_terminals.png \
	docs/wsbar.dia \
	docs/wsbar.png \
	i3bar/LICENSE \
	libi3/README \
	$(asciidoc_MANS:.1=.man) \
	$(asciidoc_MANS:.1=.man) \
	man/asciidoc.conf.in \
	DEPENDS \
	I3_VERSION \
	LICENSE \
	PACKAGE-MAINTAINER \
	RELEASE-NOTES-4.15 \
	generate-command-parser.pl \
	parser-specs/commands.spec \
	parser-specs/config.spec \
	parser-specs/highlighting.vim \
	pseudo-doc.doxygen \
	testcases/complete-run.pl.in \
	testcases/i3-test.config \
	testcases/lib/i3test/Test.pm \
	testcases/lib/i3test/Util.pm \
	testcases/lib/i3test/XTEST.pm \
	testcases/lib/i3test.pm.in \
	testcases/lib/SocketActivation.pm \
	testcases/lib/StartXServer.pm \
	testcases/lib/StatusLine.pm \
	testcases/lib/TestWorker.pm \
	testcases/Makefile.PL \
	testcases/new-test \
	testcases/restart-state.golden \
	testcases/t \
	testcases/valgrind.supp

# dirstamps contains directories which we want to be created in $(top_builddir)
# so that our custom rules can store files in them.
dirstamp = .dirstamp
dirstamps = \
	docs/$(dirstamp) \
	man/$(dirstamp) \
	parser/$(dirstamp)
DISTCLEANFILES = $(dirstamps)

$(dirstamps):
	@stamp='$@'; $(MKDIR_P) "$${stamp%/*}"
	@: > $@

################################################################################
# docs generation
################################################################################

docs_tocdir = ${docdir}
docs_notocdir = ${docdir}
docs_poddir = ${docdir}
if BUILD_DOCS
dist_docs_toc_DATA = \
	docs/hacking-howto.html \
	docs/userguide.html \
	docs/ipc.html \
	docs/multi-monitor.html \
	docs/wsbar.html \
	docs/testsuite.html \
	docs/i3bar-protocol.html \
	docs/layout-saving.html

dist_docs_notoc_DATA = \
	docs/debugging.html

dist_docs_pod_DATA = \
	docs/lib-i3test.html \
	docs/lib-i3test-test.html

$(dist_docs_toc_DATA): docs/%.html: docs/% docs/$(dirstamp)
	$(AM_V_GEN) @PATH_ASCIIDOC@ -a toc -n -o $@ $<

$(dist_docs_notoc_DATA): docs/%.html: docs/% docs/$(dirstamp)
	$(AM_V_GEN) @PATH_ASCIIDOC@ -n -o $@ $<

docs/lib-i3test.html: testcases/lib/i3test.pm docs/$(dirstamp)
	$(AM_V_GEN) $(top_srcdir)/docs/i3-pod2html $< $@

docs/lib-i3test-test.html: testcases/lib/i3test/Test.pm docs/$(dirstamp)
	$(AM_V_GEN) $(top_srcdir)/docs/i3-pod2html $< $@

else
dist_docs_toc_DATA =
dist_docs_notoc_DATA =
dist_docs_pod_DATA =
endif

################################################################################
# manpage generation
################################################################################

if BUILD_MANS
dist_man1_MANS = \
	$(asciidoc_MANS) \
	$(pod_MANS)

asciidoc_MANS = \
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

pod_MANS = \
	man/i3-dmenu-desktop.1 \
	man/i3-save-tree.1

$(asciidoc_MANS): man/%.1: man/%.xml man/$(dirstamp)
	$(AM_V_GEN) out='$@'; @PATH_XMLTO@ man -o "$${out%/*}" $<
	@stamp='$@'; $(MKDIR_P) "$${stamp%/*}"

man/%.xml: man/%.man man/asciidoc.conf man/$(dirstamp)
	$(AM_V_GEN) @PATH_ASCIIDOC@ -d manpage -b docbook -f $(top_builddir)/man/asciidoc.conf -o $@ $<

$(pod_MANS): man/%.1: % man/$(dirstamp)
	$(AM_V_GEN) @PATH_POD2MAN@ --utf8 $< > $@
else
asciidoc_MANS =
endif

AM_CPPFLAGS = \
	-DSYSCONFDIR="\"$(sysconfdir)\"" \
	-I$(top_builddir)/parser \
	-I$(top_srcdir)/include \
	@AX_EXTEND_SRCDIR_CPPFLAGS@

i3_CFLAGS = \
	$(AM_CFLAGS) \
	$(libi3_CFLAGS) \
	$(LIBSN_CFLAGS) \
	$(XCB_CFLAGS) \
	$(XCB_UTIL_CURSOR_CFLAGS) \
	$(XCB_UTIL_KEYSYM_CFLAGS) \
	$(XCB_UTIL_WM_CFLAGS) \
	$(XCB_UTIL_XRM_CFLAGS) \
	$(XKBCOMMON_CFLAGS) \
	$(YAJL_CFLAGS) \
	$(LIBPCRE_CFLAGS) \
	$(PTHREAD_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

i3_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CODE_COVERAGE_CPPFLAGS)

i3_LDADD = \
	$(libi3_LIBS) \
	$(LIBSN_LIBS) \
	$(XCB_LIBS) \
	$(XCB_UTIL_CURSOR_LIBS) \
	$(XCB_UTIL_KEYSYMS_LIBS) \
	$(XCB_UTIL_WM_LIBS) \
	$(XCB_UTIL_XRM_LIBS) \
	$(XKBCOMMON_LIBS) \
	$(YAJL_LIBS) \
	$(LIBPCRE_LIBS) \
	$(PANGOCAIRO_LIBS) \
	$(PTHREAD_LIBS) \
	$(CODE_COVERAGE_LDFLAGS)

libi3_CFLAGS = \
	$(AM_CFLAGS) \
	$(XCB_CFLAGS) \
	$(XCB_UTIL_CFLAGS) \
	$(XCB_UTIL_XRM_CFLAGS) \
	$(YAJL_CFLAGS) \
	$(PANGOCAIRO_CFLAGS)

libi3_LIBS = \
	$(top_builddir)/libi3.a \
	$(XCB_LIBS) \
	$(XCB_UTIL_LIBS) \
	$(XCB_UTIL_XRM_LIBS) \
	$(YAJL_LIBS) \
	$(PANGOCAIRO_LIBS)

libi3_a_CFLAGS = \
	$(libi3_CFLAGS)

libi3_a_SOURCES = \
	include/libi3.h \
	libi3/dpi.c \
	libi3/draw_util.c \
	libi3/fake_configure_notify.c \
	libi3/font.c \
	libi3/format_placeholders.c \
	libi3/get_colorpixel.c \
	libi3/get_config_path.c \
	libi3/get_exe_path.c \
	libi3/get_mod_mask.c \
	libi3/get_process_filename.c \
	libi3/get_visualtype.c \
	libi3/ipc_connect.c \
	libi3/ipc_recv_message.c \
	libi3/ipc_send_message.c \
	libi3/is_debug_build.c \
	libi3/mkdirp.c \
	libi3/resolve_tilde.c \
	libi3/root_atom_contents.c \
	libi3/safewrappers.c \
	libi3/string.c \
	libi3/strndup.c \
	libi3/ucs2_conversion.c

i3_dump_log_i3_dump_log_CFLAGS = \
	$(AM_CFLAGS) \
	$(PTHREAD_CFLAGS) \
	$(libi3_CFLAGS)

i3_dump_log_i3_dump_log_LDADD = \
	$(PTHREAD_LIBS) \
	$(libi3_LIBS)

i3_dump_log_i3_dump_log_SOURCES = \
	i3-dump-log/main.c

i3_input_i3_input_CFLAGS = \
	$(AM_CFLAGS) \
	$(libi3_CFLAGS)

i3_input_i3_input_LDADD = \
	$(libi3_LIBS) \
	$(XCB_UTIL_KEYSYMS_LIBS)

i3_input_i3_input_SOURCES = \
	i3-input/i3-input.h \
	i3-input/keysym2ucs.c \
	i3-input/keysym2ucs.h \
	i3-input/main.c

i3_msg_i3_msg_CFLAGS = \
	$(AM_CFLAGS) \
	$(libi3_CFLAGS)

i3_msg_i3_msg_LDADD = \
	$(libi3_LIBS)

i3_msg_i3_msg_SOURCES = \
	i3-msg/main.c

i3_nagbar_i3_nagbar_CFLAGS = \
	$(AM_CFLAGS) \
	$(libi3_CFLAGS)

i3_nagbar_i3_nagbar_LDADD = \
	$(libi3_LIBS) \
	$(XCB_UTIL_CURSOR_LIBS)

i3_nagbar_i3_nagbar_SOURCES = \
	i3-nagbar/atoms.xmacro \
	i3-nagbar/i3-nagbar.h \
	i3-nagbar/main.c

i3bar_i3bar_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/i3bar/include

i3bar_i3bar_CFLAGS = \
	$(AM_CFLAGS) \
	$(libi3_CFLAGS) \
	$(XCB_CFLAGS) \
	$(XKBCOMMON_CFLAGS) \
	$(PANGOCAIRO_CFLAGS) \
	$(YAJL_CFLAGS)

i3bar_i3bar_LDADD = \
	$(libi3_LIBS) \
	$(XCB_LIBS) \
	$(XCB_UTIL_CURSOR_LIBS) \
	$(XKBCOMMON_LIBS) \
	$(PANGOCAIRO_LIBS) \
	$(YAJL_LIBS)

i3bar_i3bar_SOURCES = \
	i3bar/include/child.h \
	i3bar/include/common.h \
	i3bar/include/configuration.h \
	i3bar/include/ipc.h \
	i3bar/include/mode.h \
	i3bar/include/outputs.h \
	i3bar/include/parse_json_header.h \
	i3bar/include/trayclients.h \
	i3bar/include/util.h \
	i3bar/include/workspaces.h \
	i3bar/include/xcb_atoms.def \
	i3bar/include/xcb.h \
	i3bar/src/child.c \
	i3bar/src/config.c \
	i3bar/src/ipc.c \
	i3bar/src/main.c \
	i3bar/src/mode.c \
	i3bar/src/outputs.c \
	i3bar/src/parse_json_header.c \
	i3bar/src/workspaces.c \
	i3bar/src/xcb.c

i3_config_wizard_i3_config_wizard_CFLAGS = \
	$(AM_CFLAGS) \
	$(libi3_CFLAGS) \
	$(XKBCOMMON_CFLAGS)

i3_config_wizard_i3_config_wizard_LDADD = \
	$(libi3_LIBS) \
	$(XCB_UTIL_KEYSYMS_LIBS) \
	$(XKBCOMMON_LIBS)

i3_config_wizard_i3_config_wizard_SOURCES = \
	i3-config-wizard/atoms.xmacro \
	i3-config-wizard/main.c \
	i3-config-wizard/xcb.h

test_inject_randr15_CPPFLAGS = \
	$(AM_CPPFLAGS)

test_inject_randr15_CFLAGS = \
	$(AM_CFLAGS) \
	$(i3_CFLAGS)

test_inject_randr15_SOURCES = \
	testcases/inject_randr1.5.c

test_inject_randr15_LDADD = \
	$(i3_LDADD)

test_commands_parser_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DTEST_PARSER

test_commands_parser_CFLAGS = \
	$(AM_CFLAGS) \
	$(i3_CFLAGS)

test_commands_parser_SOURCES = \
	src/commands_parser.c

test_commands_parser_LDADD = \
	$(i3_LDADD)

test_config_parser_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DTEST_PARSER

test_config_parser_CFLAGS = \
	$(AM_CFLAGS) \
	$(i3_CFLAGS)

test_config_parser_SOURCES = \
	src/config_parser.c

test_config_parser_LDADD = \
	$(i3_LDADD)

command_parser_SOURCES = \
	parser/GENERATED_command_enums.h \
	parser/GENERATED_command_tokens.h \
	parser/GENERATED_command_call.h

config_parser_SOURCES = \
	parser/GENERATED_config_enums.h \
	parser/GENERATED_config_tokens.h \
	parser/GENERATED_config_call.h

i3_SOURCES = \
	$(command_parser_SOURCES) \
	$(config_parser_SOURCES) \
	include/all.h \
	include/assignments.h \
	include/atoms_NET_SUPPORTED.xmacro \
	include/atoms_rest.xmacro \
	include/atoms.xmacro \
	include/bindings.h \
	include/click.h \
	include/cmdparse.h \
	include/commands.h \
	include/commands_parser.h \
	include/config_directives.h \
	include/configuration.h \
	include/config_parser.h \
	include/con.h \
	include/data.h \
	include/display_version.h \
	include/ewmh.h \
	include/fake_outputs.h \
	include/floating.h \
	include/handlers.h \
	include/i3.h \
	include/ipc.h \
	include/key_press.h \
	include/load_layout.h \
	include/log.h \
	include/main.h \
	include/manage.h \
	include/match.h \
	include/move.h \
	include/output.h \
	include/queue.h \
	include/randr.h \
	include/regex.h \
	include/render.h \
	include/resize.h \
	include/restore_layout.h \
	include/scratchpad.h \
	include/sd-daemon.h \
	include/shmlog.h \
	include/sighandler.h \
	include/startup.h \
	include/tree.h \
	include/util.h \
	include/window.h \
	include/workspace.h \
	include/xcb.h \
	include/xcursor.h \
	include/x.h \
	include/xinerama.h \
	include/yajl_utils.h \
	src/assignments.c \
	src/bindings.c \
	src/click.c \
	src/commands.c \
	src/commands_parser.c \
	src/con.c \
	src/config.c \
	src/config_directives.c \
	src/config_parser.c \
	src/display_version.c \
	src/ewmh.c \
	src/fake_outputs.c \
	src/floating.c \
	src/handlers.c \
	src/ipc.c \
	src/key_press.c \
	src/load_layout.c \
	src/log.c \
	src/main.c \
	src/manage.c \
	src/match.c \
	src/move.c \
	src/output.c \
	src/randr.c \
	src/regex.c \
	src/render.c \
	src/resize.c \
	src/restore_layout.c \
	src/scratchpad.c \
	src/sd-daemon.c \
	src/sighandler.c \
	src/startup.c \
	src/tree.c \
	src/util.c \
	src/version.c \
	src/window.c \
	src/workspace.c \
	src/x.c \
	src/xcb.c \
	src/xcursor.c \
	src/xinerama.c

################################################################################
# parser generation
################################################################################

$(command_parser_SOURCES): %.h: i3-command-parser.stamp

$(config_parser_SOURCES): %.h: i3-config-parser.stamp

src/i3-commands_parser.$(OBJEXT): i3-command-parser.stamp

src/i3-config_parser.$(OBJEXT): i3-config-parser.stamp

i3-command-parser.stamp: parser/$(dirstamp) generate-command-parser.pl parser-specs/commands.spec
	$(AM_V_GEN) $(top_srcdir)/generate-command-parser.pl --input=$(top_srcdir)/parser-specs/commands.spec --prefix=command
	$(AM_V_at) mv GENERATED_command_* $(top_builddir)/parser
	$(AM_V_at) touch $@

i3-config-parser.stamp: parser/$(dirstamp) generate-command-parser.pl parser-specs/config.spec
	$(AM_V_GEN) $(top_srcdir)/generate-command-parser.pl --input=$(top_srcdir)/parser-specs/config.spec --prefix=config
	$(AM_V_at) mv GENERATED_config_* $(top_builddir)/parser
	$(AM_V_at) touch $@

################################################################################
# AnyEvent-I3 build process
################################################################################

anyevent-i3.stamp: AnyEvent-I3/lib/AnyEvent/I3.pm
	$(AM_V_BUILD) (cd $(top_srcdir)/AnyEvent-I3 && perl Makefile.PL && make)
	$(AM_V_at) touch $@

CLEANFILES = \
	i3-command-parser.stamp \
	i3-config-parser.stamp \
	anyevent-i3.stamp
