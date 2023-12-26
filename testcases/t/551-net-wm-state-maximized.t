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
# Tests for setting and removing the _NET_WM_STATE_MAXIMIZED_VERT and
# _NET_WM_STATE_MAXIMIZED_HORZ atoms.
use i3test;
use X11::XCB qw(:all);

my $windowA;
fresh_workspace;
$windowA = open_window;

ok(!net_wm_state_contains($windowA, '_NET_WM_STATE_MAXIMIZED_VERT'),
   'i3 does not currently set _NET_WM_STATE_MAXIMIZED_VERT');

ok(!net_wm_state_contains($windowA, '_NET_WM_STATE_MAXIMIZED_HORZ'),
   'i3 does not currently set _NET_WM_STATE_MAXIMIZED_HORZ');

done_testing;
