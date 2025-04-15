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
# Tests sticky windows.
# Ticket: #1455
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

workspace ws-on-0 output fake-0

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

my ($ws, $tmp, $focused);

###############################################################################
# 1: Given a sticky tiling container, when the workspace is switched, then
#    nothing happens.
###############################################################################
fresh_workspace;
open_window;
cmd 'sticky enable';
$ws = fresh_workspace;

is(@{get_ws($ws)->{nodes}}, 0, 'tiling sticky container did not move');
is(@{get_ws($ws)->{floating_nodes}}, 0, 'tiling sticky container did not move');
kill_all_windows;

###############################################################################
# 2: Given a sticky floating container, when the workspace is switched, then
#    the container moves to the new workspace.
###############################################################################
$ws = fresh_workspace;
open_floating_window;
$focused = get_focused($ws);
cmd 'sticky enable';
$ws = fresh_workspace;

is(@{get_ws($ws)->{floating_nodes}}, 1, 'floating sticky container moved to new workspace');
is(get_focused($ws), $focused, 'sticky container has focus');
kill_all_windows;

###############################################################################
# 3: Given two sticky floating containers, when the workspace is switched,
#    then both containers move to the new workspace.
###############################################################################
fresh_workspace;
open_floating_window;
cmd 'sticky enable';
open_floating_window;
cmd 'sticky enable';
$ws = fresh_workspace;

is(@{get_ws($ws)->{floating_nodes}}, 2, 'multiple sticky windows can be used at the same time');
kill_all_windows;

###############################################################################
# 4: Given an unfocused sticky floating container and a tiling container on the
#    target workspace, when the workspace is switched, then the tiling container
#    is focused.
###############################################################################
$ws = fresh_workspace;
open_window;
$focused = get_focused($ws);
fresh_workspace;
open_floating_window;
cmd 'sticky enable';
open_window;
cmd 'workspace ' . $ws;

is(get_focused($ws), $focused, 'the tiling container has focus');
kill_all_windows;

###############################################################################
# 5: Given a focused sticky floating container and a tiling container on the
#    target workspace, when the workspace is switched, then the tiling container
#    is focused.
###############################################################################
$ws = fresh_workspace;
open_window;
$tmp = fresh_workspace;
open_floating_window;
$focused = get_focused($tmp);
cmd 'sticky enable';
cmd 'workspace ' . $ws;

is(get_focused($ws), $focused, 'the sticky container has focus');
kill_all_windows;

###############################################################################
# 6: Given a floating container on a non-visible workspace, when the window
#    is made sticky, then the window immediately jumps to the currently
#    visible workspace.
###############################################################################
fresh_workspace;
open_floating_window;
cmd 'mark sticky';
$ws = fresh_workspace;
cmd '[con_mark=sticky] sticky enable';

is(@{get_ws($ws)->{floating_nodes}}, 1, 'the sticky window jumps to the front');
kill_all_windows;

###############################################################################
# 7: Given a sticky floating container and a workspace on another output, when
#    a new workspace assigned to the first output is focused, then the sticky
#    container should jump to the new workspace and have input focus correctly.
###############################################################################
$ws = fresh_workspace(output => 0);
open_floating_window;
cmd 'sticky enabled';
$focused = get_focused($ws);
$ws = fresh_workspace(output => 1);

is(@{get_ws($ws)->{floating_nodes}}, 0, 'the sticky window didn\'t jump to a workspace on a different output');
$ws = 'ws-on-0';
cmd "workspace $ws";
is(@{get_ws($ws)->{floating_nodes}}, 1, 'the sticky window moved to new workspace on first output');
is(get_focused($ws), $focused, 'the sticky window has focus');
kill_all_windows;

###############################################################################

done_testing;
