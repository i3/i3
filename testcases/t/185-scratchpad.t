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
# Tests for the scratchpad functionality.
#
use i3test;
use List::Util qw(first);

my $i3 = i3(get_socket_path());
my $tmp = fresh_workspace;

################################################################################
# 1: Verify that the __i3 output contains the __i3_scratch workspace and that
# it’s empty initially. Also, __i3 should not show up in GET_OUTPUTS so that
# tools like i3bar will not handle it. Similarly, __i3_scratch should not show
# up in GET_WORKSPACES. After all, you should not be able to switch to it.
################################################################################

my $tree = $i3->get_tree->recv;
is($tree->{name}, 'root', 'root node is the first thing we get');

my @__i3 = grep { $_->{name} eq '__i3' } @{$tree->{nodes}};
is(scalar @__i3, 1, 'output __i3 found');

my $content = first { $_->{type} eq 'con' } @{$__i3[0]->{nodes}};
my @workspaces = @{$content->{nodes}};
my @workspace_names = map { $_->{name} } @workspaces;
ok('__i3_scratch' ~~ @workspace_names, '__i3_scratch workspace found');

my $get_outputs = $i3->get_outputs->recv;
my $get_ws = $i3->get_workspaces->recv;
my @output_names = map { $_->{name} } @$get_outputs;
my @ws_names = map { $_->{name} } @$get_ws;

ok(!('__i3' ~~ @output_names), '__i3 not in GET_OUTPUTS');
ok(!('__i3_scratch' ~~ @ws_names), '__i3_scratch ws not in GET_WORKSPACES');

################################################################################
# 2: Verify that you cannot switch to the __i3_scratch workspace and moving
# windows to __i3_scratch does not work (users should be aware of the different
# behavior and acknowledge that by using the scratchpad commands).
################################################################################

# Try focusing the workspace.
my $__i3_scratch = get_ws('__i3_scratch');
is($__i3_scratch->{focused}, 0, '__i3_scratch ws not focused');

cmd 'workspace __i3_scratch';

$__i3_scratch = get_ws('__i3_scratch');
is($__i3_scratch->{focused}, 0, '__i3_scratch ws still not focused');


# Try moving a window to it.
is(scalar @{$__i3_scratch->{floating_nodes}}, 0, '__i3_scratch ws empty');

my $window = open_window;
cmd 'move workspace __i3_scratch';

$__i3_scratch = get_ws('__i3_scratch');
is(scalar @{$__i3_scratch->{floating_nodes}}, 0, '__i3_scratch ws empty');


# Try moving the window with the 'output <direction>' command.
# We hardcode output left since the pseudo-output will be initialized before
# every other output, so it will always be the first one.
cmd 'move output left';

$__i3_scratch = get_ws('__i3_scratch');
is(scalar @{$__i3_scratch->{floating_nodes}}, 0, '__i3_scratch ws empty');


# Try moving the window with the 'output <name>' command.
cmd 'move output __i3';

$__i3_scratch = get_ws('__i3_scratch');
is(scalar @{$__i3_scratch->{floating_nodes}}, 0, '__i3_scratch ws empty');


################################################################################
# 3: Verify that 'scratchpad toggle' sends a window to the __i3_scratch
# workspace and sets the scratchpad flag to SCRATCHPAD_FRESH. The window’s size
# and position will be changed on the next 'scratchpad show'.
################################################################################

my ($nodes, $focus) = get_ws_content($tmp);
is(scalar @$nodes, 1, 'precisely one window on current ws');
is($nodes->[0]->{scratchpad_state}, 'none', 'scratchpad_state none');

cmd 'move scratchpad';

$__i3_scratch = get_ws('__i3_scratch');
my @scratch_nodes = @{$__i3_scratch->{floating_nodes}};
is(scalar @scratch_nodes, 1, '__i3_scratch contains our window');
($nodes, $focus) = get_ws_content($tmp);
is(scalar @$nodes, 0, 'no window on current ws anymore');

is($scratch_nodes[0]->{scratchpad_state}, 'fresh', 'scratchpad_state fresh');

$tree = $i3->get_tree->recv;
my $__i3 = first { $_->{name} eq '__i3' } @{$tree->{nodes}};
isnt($tree->{focus}->[0], $__i3->{id}, '__i3 output not focused');

$get_outputs = $i3->get_outputs->recv;
$get_ws = $i3->get_workspaces->recv;
@output_names = map { $_->{name} } @$get_outputs;
@ws_names = map { $_->{name} } @$get_ws;

ok(!('__i3' ~~ @output_names), '__i3 not in GET_OUTPUTS');
ok(!('__i3_scratch' ~~ @ws_names), '__i3_scratch ws not in GET_WORKSPACES');

################################################################################
# 4: Verify that 'scratchpad show' makes the window visible.
################################################################################

# Open another window so that we can check if focus is on the scratchpad window
# after showing it.
my $second_window = open_window;
my $old_focus = get_focused($tmp);

cmd 'scratchpad show';

isnt(get_focused($tmp), $old_focus, 'focus changed');

$__i3_scratch = get_ws('__i3_scratch');
@scratch_nodes = @{$__i3_scratch->{floating_nodes}};
is(scalar @scratch_nodes, 0, '__i3_scratch is now empty');

my $ws = get_ws($tmp);
my $output = $tree->{nodes}->[1];
my $scratchrect = $ws->{floating_nodes}->[0]->{rect};
my $outputrect = $output->{rect};

is($scratchrect->{width}, $outputrect->{width} * 0.5, 'scratch width is 50%');
is($scratchrect->{height}, $outputrect->{height} * 0.75, 'scratch height is 75%');
is($scratchrect->{x},
   ($outputrect->{width} / 2) - ($scratchrect->{width} / 2),
   'scratch window centered horizontally');
is($scratchrect->{y},
   ($outputrect->{height} / 2 ) - ($scratchrect->{height} / 2),
   'scratch window centered vertically');

################################################################################
# 5: Another 'scratchpad show' should make that window go to the scratchpad
# again.
################################################################################

cmd 'scratchpad show';

$__i3_scratch = get_ws('__i3_scratch');
@scratch_nodes = @{$__i3_scratch->{floating_nodes}};
is(scalar @scratch_nodes, 1, '__i3_scratch contains our window');

################################################################################
# 6: Resizing the window should disable auto centering on scratchpad show
################################################################################

cmd 'scratchpad show';

$ws = get_ws($tmp);
is($ws->{floating_nodes}->[0]->{scratchpad_state}, 'fresh',
   'scratchpad_state fresh');

cmd 'resize grow width 10 px';
cmd 'scratchpad show';
cmd 'scratchpad show';

$ws = get_ws($tmp);
$scratchrect = $ws->{floating_nodes}->[0]->{rect};
$outputrect = $output->{rect};

is($ws->{floating_nodes}->[0]->{scratchpad_state}, 'changed',
   'scratchpad_state changed');
is($scratchrect->{width}, $outputrect->{width} * 0.5 + 10, 'scratch width is 50% + 10px');

cmd 'resize shrink width 10 px';
cmd 'scratchpad show';

################################################################################
# 7: Verify that repeated 'scratchpad show' cycle through the stack, that is,
# toggling a visible window should insert it at the bottom of the stack of the
# __i3_scratch workspace.
################################################################################

my $third_window = open_window(name => 'scratch-match');
cmd 'move scratchpad';

$__i3_scratch = get_ws('__i3_scratch');
@scratch_nodes = @{$__i3_scratch->{floating_nodes}};
is(scalar @scratch_nodes, 2, '__i3_scratch contains both windows');

is($scratch_nodes[0]->{scratchpad_state}, 'changed', 'changed window first');
is($scratch_nodes[1]->{scratchpad_state}, 'fresh', 'fresh window is second');

my $changed_id = $scratch_nodes[0]->{nodes}->[0]->{id};
my $fresh_id = $scratch_nodes[1]->{nodes}->[0]->{id};
is($scratch_nodes[0]->{id}, $__i3_scratch->{focus}->[0], 'changed window first');
is($scratch_nodes[1]->{id}, $__i3_scratch->{focus}->[1], 'fresh window second');

# Repeatedly use 'scratchpad show' and check that the windows are different.
cmd 'scratchpad show';

is(get_focused($tmp), $changed_id, 'focus changed');

$ws = get_ws($tmp);
$scratchrect = $ws->{floating_nodes}->[0]->{rect};
is($scratchrect->{width}, $outputrect->{width} * 0.5, 'scratch width is 50%');
is($scratchrect->{height}, $outputrect->{height} * 0.75, 'scratch height is 75%');
is($scratchrect->{x},
   ($outputrect->{width} / 2) - ($scratchrect->{width} / 2),
   'scratch window centered horizontally');
is($scratchrect->{y},
   ($outputrect->{height} / 2 ) - ($scratchrect->{height} / 2),
   'scratch window centered vertically');

cmd 'scratchpad show';

isnt(get_focused($tmp), $changed_id, 'focus changed');

cmd 'scratchpad show';

is(get_focused($tmp), $fresh_id, 'focus changed');

cmd 'scratchpad show';

isnt(get_focused($tmp), $fresh_id, 'focus changed');

################################################################################
# 8: Show it, move it around, hide it. Verify that the position is retained
# when showing it again.
################################################################################

cmd '[title="scratch-match"] scratchpad show';

isnt(get_focused($tmp), $old_focus, 'scratchpad window shown');

my $oldrect = get_ws($tmp)->{floating_nodes}->[0]->{rect};

cmd 'move left';

$scratchrect = get_ws($tmp)->{floating_nodes}->[0]->{rect};
isnt($scratchrect->{x}, $oldrect->{x}, 'x position changed');
$oldrect = $scratchrect;

# hide it, then show it again
cmd '[title="scratch-match"] scratchpad show';
cmd '[title="scratch-match"] scratchpad show';

# verify the position is still the same
$scratchrect = get_ws($tmp)->{floating_nodes}->[0]->{rect};

is_deeply($scratchrect, $oldrect, 'position/size the same');

# hide it again for the next test
cmd '[title="scratch-match"] scratchpad show';

is(get_focused($tmp), $old_focus, 'scratchpad window hidden');

is(scalar @{get_ws($tmp)->{nodes}}, 1, 'precisely one window on current ws');

################################################################################
# 9: restart i3 and verify that the scratchpad show still works
################################################################################

$__i3_scratch = get_ws('__i3_scratch');
my $old_nodes = scalar @{$__i3_scratch->{nodes}};
my $old_floating_nodes = scalar @{$__i3_scratch->{floating_nodes}};

cmd 'restart';

does_i3_live;

$__i3_scratch = get_ws('__i3_scratch');
is(scalar @{$__i3_scratch->{nodes}}, $old_nodes, "number of nodes matches ($old_nodes)");
is(scalar @{$__i3_scratch->{floating_nodes}}, $old_floating_nodes, "number of floating nodes matches ($old_floating_nodes)");

is(scalar @{get_ws($tmp)->{nodes}}, 1, 'still precisely one window on current ws');
is(scalar @{get_ws($tmp)->{floating_nodes}}, 0, 'still no floating windows on current ws');

# verify that we can display the scratchpad window
cmd '[title="scratch-match"] scratchpad show';

$ws = get_ws($tmp);
is(scalar @{$ws->{nodes}}, 1, 'still precisely one window on current ws');
is(scalar @{$ws->{floating_nodes}}, 1, 'precisely one floating windows on current ws');
is($ws->{floating_nodes}->[0]->{scratchpad_state}, 'changed', 'scratchpad_state is "changed"');

################################################################################
# 10: on an empty workspace, ensure the 'move scratchpad' command does nothing
################################################################################

$tmp = fresh_workspace;

cmd 'move scratchpad';

does_i3_live;

################################################################################
# 11: focus a workspace and move all of its children to the scratchpad area
################################################################################

sub verify_scratchpad_move_multiple_win {
    my $floating = shift;

    my $first = open_window;
    my $second = open_window;

    if ($floating) {
        cmd 'floating toggle';
        cmd 'focus tiling';
    }

    cmd 'focus parent';
    cmd 'move scratchpad';

    does_i3_live;

    $ws = get_ws($tmp);
    is(scalar @{$ws->{nodes}}, 0, 'no windows on ws');
    is(scalar @{$ws->{floating_nodes}}, 0, 'no floating windows on ws');

    # show the first window.
    cmd 'scratchpad show';

    $ws = get_ws($tmp);
    is(scalar @{$ws->{nodes}}, 0, 'no windows on ws');
    is(scalar @{$ws->{floating_nodes}}, 1, 'one floating windows on ws');

    $old_focus = get_focused($tmp);

    cmd 'scratchpad show';

    # show the second window.
    cmd 'scratchpad show';

    $ws = get_ws($tmp);
    is(scalar @{$ws->{nodes}}, 0, 'no windows on ws');
    is(scalar @{$ws->{floating_nodes}}, 1, 'one floating windows on ws');

    isnt(get_focused($tmp), $old_focus, 'focus changed');
}

$tmp = fresh_workspace;
verify_scratchpad_move_multiple_win(0);
$tmp = fresh_workspace;
verify_scratchpad_move_multiple_win(1);

################################################################################
# 12: open a scratchpad window on a workspace, switch to another workspace and
# call 'scratchpad show' again
################################################################################

sub verify_scratchpad_move_with_visible_scratch_con {
    my ($first, $second, $cross_output) = @_;

    cmd "workspace $first";

    my $window1 = open_window;
    cmd 'move scratchpad';

    my $window2 = open_window;
    cmd 'move scratchpad';

    # this should bring up window 1
    cmd 'scratchpad show';

    $ws = get_ws($first);
    is(scalar @{$ws->{floating_nodes}}, 1, 'one floating node on ws1');
    is($x->input_focus, $window1->id, "showed the correct scratchpad window1");

    # this should bring up window 1
    cmd "workspace $second";
    cmd 'scratchpad show';
    is($x->input_focus, $window1->id, "showed the correct scratchpad window1");

    my $ws2 = get_ws($second);
    is(scalar @{$ws2->{floating_nodes}}, 1, 'one floating node on ws2');
    unless ($cross_output) {
        ok(!workspace_exists($first), 'ws1 was empty and therefore closed');
    } else {
        $ws = get_ws($first);
        is(scalar @{$ws->{floating_nodes}}, 0, 'ws1 has no floating nodes');
    }

    # hide window 1 again
    cmd 'move scratchpad';

    # this should bring up window 2
    cmd "workspace $first";
    cmd 'scratchpad show';
    is($x->input_focus, $window2->id, "showed the correct scratchpad window");
}

# let's clear the scratchpad first
sub clear_scratchpad {
    while (scalar @{get_ws('__i3_scratch')->{floating_nodes}}) {
        cmd 'scratchpad show';
        cmd 'kill';
    }
}

clear_scratchpad;
is (scalar @{get_ws('__i3_scratch')->{floating_nodes}}, 0, "scratchpad is empty");

my ($first, $second);
$first = fresh_workspace;
$second = fresh_workspace;

verify_scratchpad_move_with_visible_scratch_con($first, $second, 0);
does_i3_live;


################################################################################
# 13: Test whether scratchpad show moves focus to the scratchpad window
# when another window on the same workspace has focus
################################################################################

clear_scratchpad;
$ws = fresh_workspace;

open_window;
my $scratch = get_focused($ws);
cmd 'move scratchpad';
cmd 'scratchpad show';

open_window;
my $not_scratch = get_focused($ws);
is(get_focused($ws), $not_scratch, 'not scratch window has focus');

cmd 'scratchpad show';

is(get_focused($ws), $scratch, 'scratchpad is focused');

# TODO: make i3bar display *something* when a window on the scratchpad has the urgency hint

################################################################################
# 14: Verify that 'move scratchpad' sends floating containers to scratchpad but
# does not resize/resposition the container on the next 'scratchpad show', i.e.,
# i3 sets the scratchpad flag to SCRATCHPAD_CHANGED
################################################################################

clear_scratchpad;
$tmp = fresh_workspace;
open_window;

($nodes, $focus) = get_ws_content($tmp);
is(scalar @$nodes, 1, 'precisely one window on current ws');
is($nodes->[0]->{scratchpad_state}, 'none', 'scratchpad_state none');

cmd 'floating toggle';
cmd 'move scratchpad';

$__i3_scratch = get_ws('__i3_scratch');
@scratch_nodes = @{$__i3_scratch->{floating_nodes}};
is(scalar @scratch_nodes, 1, '__i3_scratch contains our window');
($nodes, $focus) = get_ws_content($tmp);
is(scalar @$nodes, 0, 'no window on current ws anymore');

is($scratch_nodes[0]->{scratchpad_state}, 'changed', 'scratchpad_state changed');

done_testing;
