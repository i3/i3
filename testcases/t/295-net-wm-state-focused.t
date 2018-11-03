#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Tests for setting and removing the _NET_WM_STATE_FOCUSED atom properly.
# Ticket: #2273
use i3test;
use X11::XCB qw(:all);

my ($windowA, $windowB);

fresh_workspace;
$windowA = open_window;
ok(is_net_wm_state_focused($windowA), 'a newly opened window that is focused should have _NET_WM_STATE_FOCUSED set');

$windowB = open_window;
ok(!is_net_wm_state_focused($windowA), 'when a another window is focused, the old window should not have _NET_WM_STATE_FOCUSED set');
ok(is_net_wm_state_focused($windowB), 'a newly opened window that is focused should have _NET_WM_STATE_FOCUSED set');

# See issue #3495.
cmd 'kill';
ok(is_net_wm_state_focused($windowA), 'when the second window is closed, the first window should have _NET_WM_STATE_FOCUSED set');

fresh_workspace;
ok(!is_net_wm_state_focused($windowA), 'when focus moves to the ewmh support window, no window should have _NET_WM_STATE_FOCUSED set');

done_testing;
