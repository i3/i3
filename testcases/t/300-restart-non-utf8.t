#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • https://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • https://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • https://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Verify that i3 does not crash when restart is issued while a window with a
# title that contains non-UTF8 characters is open.
# Ticket: #3156
# Bug still in: 4.15-241-g9dc4df81
use i3test;

my $ws = fresh_workspace;
open_window(name => "\x{AA} <-- invalid");

cmd 'restart';
does_i3_live;

# Confirm that the invalid character got replaced with U+FFFD - "REPLACEMENT
# CHARACTER"
cmd '[title="^' . "\x{fffd}" . ' <-- invalid$"] fullscreen enable';
is_num_fullscreen($ws, 1, 'title based criterion works');

done_testing;
