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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Tests for setting and removing the _NET_WM_STATE_HIDDEN atom properly.
# Ticket: #1648
use i3test;
use X11::XCB qw(:all);

sub is_hidden {
    my ($con) = @_;
    return net_wm_state_contains($con, '_NET_WM_STATE_HIDDEN');
}

my ($tabA, $tabB, $tabC, $subtabA, $subtabB, $windowA, $windowB);

###############################################################################
# Given two containers next to each other, when focusing one, then the other
# one does not have _NET_WM_STATE_HIDDEN set.
###############################################################################

fresh_workspace;
$windowA = open_window;
$windowB = open_window;

ok(!is_hidden($windowA), 'left window does not have _NET_WM_STATE_HIDDEN set');
ok(!is_hidden($windowB), 'right window does not have _NET_WM_STATE_HIDDEN set');

###############################################################################
# Given two containers on different workspaces, when one is focused, then
# the other one does not have _NET_WM_STATE_HIDDEN set.
###############################################################################

fresh_workspace;
$windowA = open_window;
fresh_workspace;
$windowB = open_window;

ok(!is_hidden($windowA), 'left window does not have _NET_WM_STATE_HIDDEN set');
ok(!is_hidden($windowB), 'right window does not have _NET_WM_STATE_HIDDEN set');

###############################################################################
# Given two containers in the same tabbed container, when one is focused, then
# (only) the other one has _NET_WM_STATE_HIDDEN set.
# Given the other tab is focused, then the atom is transferred.
###############################################################################

fresh_workspace;
$tabA = open_window;
cmd 'layout tabbed';
$tabB = open_window;

ok(is_hidden($tabA), 'unfocused tab has _NET_WM_STATE_HIDDEN set');
ok(!is_hidden($tabB), 'focused tab does not have _NET_WM_STATE_HIDDEN set');

cmd 'focus left';

ok(!is_hidden($tabA), 'focused tab does not have _NET_WM_STATE_HIDDEN set');
ok(is_hidden($tabB), 'unfocused tab has _NET_WM_STATE_HIDDEN set');

###############################################################################
# Given three containers in the same stacked container, when the focused tab
# is moved to another workspace, then the now focused tab does not have
# _NET_WM_STATE_HIDDEN set anymore.
###############################################################################

fresh_workspace;
$tabA = open_window;
cmd 'layout stacked';
$tabB = open_window;
$tabC = open_window;
cmd 'move window to workspace unused';

ok(is_hidden($tabA), 'unfocused tab has _NET_WM_STATE_HIDDEN set');
ok(!is_hidden($tabB), 'focused tab does not have _NET_WM_STATE_HIDDEN set');
ok(!is_hidden($tabC), 'moved window does not have _NET_WM_STATE_HIDDEN set');

###############################################################################
# Given three containers in the same stacked container, when a not focused
# tab is moved to another workspace, then it does not have _NET_WM_STATE_HIDDEN
# set anymore.
###############################################################################

fresh_workspace;
$tabA = open_window;
cmd 'layout stacked';
$tabB = open_window;
cmd 'mark moveme';
$tabC = open_window;
cmd '[con_mark="moveme"] move window to workspace unused';

ok(is_hidden($tabA), 'unfocused tab has _NET_WM_STATE_HIDDEN set');
ok(!is_hidden($tabB), 'moved window does not have _NET_WM_STATE_HIDDEN set');
ok(!is_hidden($tabC), 'focused tab does not have _NET_WM_STATE_HIDDEN set');

###############################################################################
# Given a tabbed container and some other container, when the latter is moved
# into the tabbed container, then all other tabs have _NET_WM_STATE_HIDDEN
# set.
###############################################################################

fresh_workspace;
$tabA = open_window;
cmd 'layout tabbed';
$tabB = open_window;
cmd 'focus parent';
cmd 'split h';
$tabC = open_window;
cmd 'move left';

ok(is_hidden($tabA), 'unfocused tab has _NET_WM_STATE_HIDDEN set');
ok(is_hidden($tabB), 'unfocused tab has _NET_WM_STATE_HIDDEN set');
ok(!is_hidden($tabC), 'focused tab does not have _NET_WM_STATE_HIDDEN set');

###############################################################################
# Given a stacked container nested inside another tabbed container with the
# inner one being in the currently focused tab, then the focused tab of the
# inner container does not have _NET_WM_STATE_HIDDEN set.
###############################################################################

fresh_workspace;
$tabA = open_window;
cmd 'layout tabbed';
$tabB = open_window;
cmd 'split h';
open_window;
cmd 'split v';
cmd 'layout stacked';
$subtabA = open_window;
$subtabB = open_window;

ok(is_hidden($tabA), 'unfocused outer tab has _NET_WM_STATE_HIDDEN set');
ok(!is_hidden($tabB), 'focused outer tab does not have _NET_WM_STATE_HIDDEN set');
ok(is_hidden($subtabA), 'unfocused inner tab has _NET_WM_STATE_HIDDEN set');
ok(!is_hidden($subtabB), 'focused inner tab does not have _NET_WM_STATE_HIDDEN set');

cmd 'focus left';

ok(!is_hidden($subtabB), 'focused inner tab does not have _NET_WM_STATE_HIDDEN set');

###############################################################################
# Given a stacked container nested inside another tabbed container with the
# inner one being in a currently not focused tab, then all tabs of the inner
# container have _NET_WM_STATE_HIDDEN set.
###############################################################################

fresh_workspace;
$tabA = open_window;
cmd 'layout tabbed';
$tabB = open_window;
cmd 'split h';
open_window;
cmd 'split v';
cmd 'layout stacked';
$subtabA = open_window;
$subtabB = open_window;
cmd 'focus parent';
cmd 'focus parent';
cmd 'focus left';

ok(!is_hidden($tabA), 'focused outer tab does not have _NET_WM_STATE_HIDDEN set');
ok(is_hidden($tabB), 'unfocused outer tab has _NET_WM_STATE_HIDDEN set');
ok(is_hidden($subtabA), 'unfocused inner tab has _NET_WM_STATE_HIDDEN set');
ok(is_hidden($subtabB), 'unfocused inner tab has _NET_WM_STATE_HIDDEN set');

###############################################################################

done_testing;
