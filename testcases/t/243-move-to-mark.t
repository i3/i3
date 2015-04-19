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
# Tests for the 'move [window|container] to mark' command
# Ticket: #1643
use i3test;

# In the following tests descriptions, we will always use the following names:
#  * 'S' for the source container which is going to be moved,
#  * 'M' for the marked target container to which 'S' will be moved.

my ($A, $B, $S, $M, $F, $source_ws, $target_ws, $ws);
my ($nodes, $focus);
my $cmd_result;

my $_NET_WM_STATE_REMOVE = 0;
my $_NET_WM_STATE_ADD = 1;
my $_NET_WM_STATE_TOGGLE = 2;

sub set_urgency {
    my ($win, $urgent_flag) = @_; 
    my $msg = pack "CCSLLLLLL",
        X11::XCB::CLIENT_MESSAGE, # response_type
        32, # format
        0, # sequence
        $win->id, # window
        $x->atom(name => '_NET_WM_STATE')->id, # message type
        ($urgent_flag ? $_NET_WM_STATE_ADD : $_NET_WM_STATE_REMOVE), # data32[0]
        $x->atom(name => '_NET_WM_STATE_DEMANDS_ATTENTION')->id, # data32[1]
        0, # data32[2]
        0, # data32[3]
        0; # data32[4]

    $x->send_event(0, $x->get_root_window(), X11::XCB::EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);
}

###############################################################################
# Given 'M' and 'S' in a horizontal split, when 'S' is moved to 'M', then
# verify that nothing changed.
###############################################################################

$ws = fresh_workspace;
$M = open_window;
cmd 'mark target';
$S = open_window;

cmd 'move container to mark target';
sync_with_i3;

($nodes, $focus) = get_ws_content($ws);
is(@{$nodes}, 2, 'there are two containers');
is($nodes->[0]->{window}, $M->{id}, 'M is left of S');
is($nodes->[1]->{window}, $S->{id}, 'S is right of M');

###############################################################################
# Given 'S' and 'M' in a horizontal split, when 'S' is moved to 'M', then
# both containers switch places.
###############################################################################

$ws = fresh_workspace;
$S = open_window;
$M = open_window;
cmd 'mark target';
cmd 'focus left';

cmd 'move container to mark target';
sync_with_i3;

($nodes, $focus) = get_ws_content($ws);
is(@{$nodes}, 2, 'there are two containers');
is($nodes->[0]->{window}, $M->{id}, 'M is left of S');
is($nodes->[1]->{window}, $S->{id}, 'S is right of M');

###############################################################################
# Given 'S' and no container 'M' exists, when 'S' is moved to 'M', then
# the command is unsuccessful.
###############################################################################

$ws = fresh_workspace;
$S = open_window;

$cmd_result = cmd 'move container to mark absent';

is($cmd_result->[0]->{success}, 0, 'command was unsuccessful');

###############################################################################
# Given 'S' and 'M' on different workspaces, when 'S' is moved to 'M', then
# 'S' ends up on the same workspace as 'M'.
###############################################################################

$source_ws = fresh_workspace;
$S = open_window;
$target_ws = fresh_workspace;
$M = open_window;
cmd 'mark target';

cmd '[id="' . $S->{id} . '"] move container to mark target';
sync_with_i3;

($nodes, $focus) = get_ws_content($source_ws);
is(@{$nodes}, 0, 'source workspace is empty');

($nodes, $focus) = get_ws_content($target_ws);
is(@{$nodes}, 2, 'both containers are on the target workspace');
is($nodes->[0]->{window}, $M->{id}, 'M is left of S');
is($nodes->[1]->{window}, $S->{id}, 'S is right of M');

###############################################################################
# Given 'S' and 'M' on different workspaces and 'S' is urgent, when 'S' is 
# moved to 'M', then the urgency flag is transferred to the target workspace.
###############################################################################

$source_ws = fresh_workspace;
$S = open_window;
$F = open_window;
$target_ws = fresh_workspace;
$M = open_window;
cmd 'mark target';
cmd 'workspace ' . $source_ws;
set_urgency($S, 1);

cmd '[id="' . $S->{id} . '"] move container to mark target';
sync_with_i3;

$source_ws = get_ws($source_ws);
$target_ws = get_ws($target_ws);
ok(!$source_ws->{urgent}, 'source workspace is no longer urgent');
ok($target_ws->{urgent}, 'target workspace is urgent');

###############################################################################
# Given 'S' and 'M' where 'M' is inside a tabbed container, when 'S' is moved
# to 'M', then 'S' ends up as a new tab.
###############################################################################

$source_ws = fresh_workspace;
$S = open_window;

# open tabbed container ['A' 'M' 'B']
$target_ws = fresh_workspace;
$A = open_window;
cmd 'layout tabbed';
$M = open_window;
cmd 'mark target';
$B = open_window;

cmd '[id="' . $S->{id} . '"] move container to mark target';
sync_with_i3;

($nodes, $focus) = get_ws_content($target_ws);
is(@{$nodes}, 1, 'there is a tabbed container');

$nodes = $nodes->[0]->{nodes};
is(@{$nodes}, 4, 'all four containers are on the target workspace');
is($nodes->[0]->{window}, $A->{id}, 'A is the first tab');
is($nodes->[1]->{window}, $M->{id}, 'M is the second tab');
is($nodes->[2]->{window}, $S->{id}, 'S is the third tab');
is($nodes->[3]->{window}, $B->{id}, 'B is the fourth tab');

###############################################################################
# Given 'S' and 'M' where 'M' is a tabbed container where the currently focused
# tab is a nested layout, when 'S' is moved to 'M', then 'S' is a new tab
# within 'M'.
###############################################################################

$source_ws = fresh_workspace;
$S = open_window;

$target_ws = fresh_workspace;
$A = open_window;
cmd 'layout tabbed';
cmd 'focus parent';
cmd 'mark target';
cmd 'focus child';
$B = open_window;
cmd 'split h';
$F = open_window;

cmd '[id="' . $S->{id} . '"] move container to mark target';
sync_with_i3;

($nodes, $focus) = get_ws_content($target_ws);
is(@{$nodes}, 1, 'there is a tabbed container');

$nodes = $nodes->[0]->{nodes};
is(@{$nodes}, 3, 'there are three tabs');

is($nodes->[0]->{window}, $A->{id}, 'A is the first tab');
is($nodes->[2]->{window}, $S->{id}, 'S is the third tab');

###############################################################################
# Given 'S' and 'M' where 'M' is inside a split container inside a tabbed
# container, when 'S' is moved to 'M', then 'S' ends up as a container
# within the same tab as 'M'.
###############################################################################

$source_ws = fresh_workspace;
$S = open_window;

# open tabbed container ['A'['B' 'M']]
$target_ws = fresh_workspace;
$A = open_window;
cmd 'layout tabbed';
$B = open_window;
cmd 'split h';
$M = open_window;
cmd 'mark target';

cmd '[id="' . $S->{id} . '"] move container to mark target';
sync_with_i3;

($nodes, $focus) = get_ws_content($target_ws);
is(@{$nodes}, 1, 'there is a tabbed container');

$nodes = $nodes->[0]->{nodes};
is(@{$nodes}, 2, 'there are two tabs');

$nodes = $nodes->[1]->{nodes};
is(@{$nodes}, 3, 'the tab with the marked children has three children');
is($nodes->[0]->{window}, $B->{id}, 'B is the first tab');
is($nodes->[1]->{window}, $M->{id}, 'M is the second tab');
is($nodes->[2]->{window}, $S->{id}, 'S is the third tab');

###############################################################################
# Given 'S', 'A' and 'B' where 'A' and 'B' are inside the tabbed container 'M',
# when 'S' is moved to 'M', then 'S' ends up as a new tab in 'M'.
###############################################################################

$source_ws = fresh_workspace;
$S = open_window;
$target_ws = fresh_workspace;
$A = open_window;
cmd 'layout tabbed';
$B = open_window;
cmd 'focus parent';
cmd 'mark target';
cmd 'focus child';

cmd '[id="' . $S->{id} . '"] move container to mark target';
sync_with_i3;

($nodes, $focus) = get_ws_content($target_ws);
is(@{$nodes}, 1, 'there is a tabbed container');

$nodes = $nodes->[0]->{nodes};
is(@{$nodes}, 3, 'there are three tabs');

is($nodes->[0]->{window}, $A->{id}, 'A is the first tab');
is($nodes->[1]->{window}, $B->{id}, 'B is the second tab');
is($nodes->[2]->{window}, $S->{id}, 'S is the third tab');

###############################################################################
# Given 'S', 'A', 'F' and 'M', where 'M' is a workspace containing a tabbed
# container, when 'S' is moved to 'M', then 'S' does not end up as a tab, but
# rather as a new window next to the tabbed container.
###############################################################################

$source_ws = fresh_workspace;
$S = open_window;
$target_ws = fresh_workspace;
$A = open_window;
cmd 'layout tabbed';
$F = open_window;
$M = $target_ws;
cmd 'focus parent';
cmd 'focus parent';
cmd 'mark target';
cmd 'focus ' . $source_ws;

cmd '[id="' . $S->{id} . '"] move container to mark target';
sync_with_i3;

($nodes, $focus) = get_ws_content($target_ws);
is(@{$nodes}, 2, 'there is a tabbed container and a window');
is($nodes->[1]->{window}, $S->{id}, 'S is the second window');

###############################################################################
# Given 'S' and 'M' where 'S' is floating and 'M' on a different workspace,
# when 'S' is moved to 'M', then 'S' is a floating container on the same
# workspaces as 'M'.
###############################################################################

$source_ws = fresh_workspace;
$S = open_floating_window;
$target_ws = fresh_workspace;
$M = open_window;
cmd 'mark target';

cmd '[id="' . $S->{id} . '"] move container to mark target';
sync_with_i3;

is(@{get_ws($target_ws)->{floating_nodes}}, 1, 'target workspace has the container now');

###############################################################################
# Given 'S' and 'M' where 'M' is floating and on a different workspace,
# when 'S' is moved to 'M', then 'S' ends up as a tiling container on the
# same workspace as 'M'.
###############################################################################

$source_ws = fresh_workspace;
$S = open_window;
$target_ws = fresh_workspace;
$M = open_floating_window;
cmd 'mark target';

cmd '[id="' . $S->{id} . '"] move container to mark target';
sync_with_i3;

($nodes, $focus) = get_ws_content($target_ws);
is(@{$nodes}, 1, 'tiling container moved to the target workspace');

###############################################################################
# Given 'S' and 'M' are the same container, when 'S' is moved to 'M', then
# the command is ignored.
###############################################################################

$ws = fresh_workspace;
$S = open_window;
$M = $S;
cmd 'mark target';

cmd '[id="' . $S->{id} . '"] move container to mark target';
sync_with_i3;

does_i3_live;

###############################################################################

done_testing;
