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
# Tests sticky windows.
# Ticket: #1455
use i3test;

my ($ws, $focused);

###############################################################################
# 1: Given a sticky tiling container, when the workspace is switched, then
#    nothing happens.
###############################################################################
fresh_workspace;
open_window(wm_class => 'findme');
cmd 'sticky enable';
$ws = fresh_workspace;

is(@{get_ws($ws)->{nodes}}, 0, 'tiling sticky container did not move');
is(@{get_ws($ws)->{floating_nodes}}, 0, 'tiling sticky container did not move');
cmd '[class="findme"] kill';

###############################################################################
# 2: Given a sticky floating container, when the workspace is switched, then
#    the container moves to the new workspace.
###############################################################################
$ws = fresh_workspace;
open_floating_window(wm_class => 'findme');
$focused = get_focused($ws);
cmd 'sticky enable';
$ws = fresh_workspace;

is(@{get_ws($ws)->{floating_nodes}}, 1, 'floating sticky container moved to new workspace');
is(get_focused($ws), $focused, 'sticky container has focus');
cmd '[class="findme"] kill';

###############################################################################
# 3: Given two sticky floating containers, when the workspace is switched,
#    then both containers move to the new workspace.
###############################################################################
fresh_workspace;
open_floating_window(wm_class => 'findme');
cmd 'sticky enable';
open_floating_window(wm_class => 'findme');
cmd 'sticky enable';
$ws = fresh_workspace;

is(@{get_ws($ws)->{floating_nodes}}, 2, 'multiple sticky windows can be used at the same time');
cmd '[class="findme"] kill';

###############################################################################
# 4: Given a sticky floating container and a tiling container on the target
#    workspace, when the workspace is switched, then the tiling container is
#    focused.
###############################################################################
$ws = fresh_workspace;
open_window;
$focused = get_focused($ws);
fresh_workspace;
open_floating_window(wm_class => 'findme');
cmd 'sticky enable';
cmd 'workspace ' . $ws;

is(get_focused($ws), $focused, 'the tiling container has focus');
cmd '[class="findme"] kill';

###############################################################################
# 5: Given a floating container on a non-visible workspace, when the window
#    is made sticky, then the window immediately jumps to the currently
#    visible workspace.
###############################################################################
fresh_workspace;
open_floating_window(wm_class => 'findme');
cmd 'mark sticky';
$ws = fresh_workspace;
cmd '[con_mark=sticky] sticky enable';

is(@{get_ws($ws)->{floating_nodes}}, 1, 'the sticky window jumps to the front');
cmd '[class="findme"] kill';

###############################################################################

done_testing;
