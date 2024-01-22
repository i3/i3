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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Tests for setting and removing the _NET_WM_STATE_FOCUSED atom properly.
# Ticket: #2273
use i3test;
use X11::XCB qw(:all);

my ($windowA, $windowB);

fresh_workspace;
$windowA = open_window;
ok(net_wm_state_contains($windowA, '_NET_WM_STATE_FOCUSED'),
   'a newly opened window that is focused should have _NET_WM_STATE_FOCUSED set');

$windowB = open_window;
ok(!net_wm_state_contains($windowA, '_NET_WM_STATE_FOCUSED'),
   'when a another window is focused, the old window should not have _NET_WM_STATE_FOCUSED set');
ok(net_wm_state_contains($windowB, '_NET_WM_STATE_FOCUSED'),
   'a newly opened window that is focused should have _NET_WM_STATE_FOCUSED set');

# See issue #3495.
cmd 'kill';
ok(net_wm_state_contains($windowA, '_NET_WM_STATE_FOCUSED'),
   'when the second window is closed, the first window should have _NET_WM_STATE_FOCUSED set');

fresh_workspace;
ok(!net_wm_state_contains($windowA, '_NET_WM_STATE_FOCUSED'),
   'when focus moves to the ewmh support window, no window should have _NET_WM_STATE_FOCUSED set');

done_testing;
