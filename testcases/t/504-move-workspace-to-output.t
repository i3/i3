#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests whether the 'move workspace <ws> to [output] <output>' command works
#
use List::Util qw(first);
use i3test;

# TODO:
# introduce 'move workspace 3 to output <output>' with synonym 'move workspace 3 to <output>'

################################################################################
# Setup workspaces so that they stay open (with an empty container).
################################################################################

is(focused_ws, '1', 'starting on workspace 1');
# ensure workspace 1 stays open
open_window;

cmd 'focus output right';
is(focused_ws, '2', 'workspace 2 on second output');
# ensure workspace 2 stays open
open_window;

cmd 'focus output right';
is(focused_ws, '1', 'back on workspace 1');

# We don’t use fresh_workspace with named workspaces here since they come last
# when using 'workspace next'.
cmd 'workspace 5';
# ensure workspace 5 stays open
open_window;

################################################################################
# Move a workspace over and verify that it is on the right output.
################################################################################

# The current order should be:
# output 1: 1, 5
# output 2: 2
cmd 'workspace 5';
is(focused_ws, '5', 'workspace 5 focused');

my ($x0, $x1) = workspaces_per_screen();
ok('5' ~~ @$x0, 'workspace 5 on xinerama-0');

cmd 'move workspace to output xinerama-1';

sub workspaces_per_screen {
    my $i3 = i3(get_socket_path());
    my $tree = $i3->get_tree->recv;
    my @outputs = @{$tree->{nodes}};

    my $xinerama0 = first { $_->{name} eq 'xinerama-0' } @outputs;
    my $xinerama0_content = first { $_->{type} == 2 } @{$xinerama0->{nodes}};

    my $xinerama1 = first { $_->{name} eq 'xinerama-1' } @outputs;
    my $xinerama1_content = first { $_->{type} == 2 } @{$xinerama1->{nodes}};

    my @xinerama0_workspaces = map { $_->{name} } @{$xinerama0_content->{nodes}};
    my @xinerama1_workspaces = map { $_->{name} } @{$xinerama1_content->{nodes}};

    return \@xinerama0_workspaces, \@xinerama1_workspaces;
}

($x0, $x1) = workspaces_per_screen();
ok('5' ~~ @$x1, 'workspace 5 now on xinerama-1');

################################################################################
# Verify that a new workspace will be created when moving the last workspace.
################################################################################

is_deeply($x0, [ '1' ], 'only workspace 1 remaining on xinerama-0');

cmd 'workspace 1';
cmd 'move workspace to output xinerama-1';

($x0, $x1) = workspaces_per_screen();
ok('1' ~~ @$x1, 'workspace 1 now on xinerama-1');
is_deeply($x0, [ '3' ], 'workspace 2 created on xinerama-0');

################################################################################
# Verify that 'move workspace to output <direction>' works
################################################################################

cmd 'workspace 5';
cmd 'move workspace to output left';

($x0, $x1) = workspaces_per_screen();
ok('5' ~~ @$x0, 'workspace 5 back on xinerama-0');

################################################################################
# Verify that coordinates of floating windows are fixed correctly when moving a
# workspace to a different output.
################################################################################

cmd 'workspace 5';
my $floating_window = open_floating_window;

my $old_rect = $floating_window->rect;

cmd 'move workspace to output right';

my $new_rect = $floating_window->rect;

isnt($old_rect->{x}, $new_rect->{x}, 'x coordinate changed');
is($old_rect->{y}, $new_rect->{y}, 'y coordinate unchanged');
is($old_rect->{width}, $new_rect->{width}, 'width unchanged');
is($old_rect->{height}, $new_rect->{height}, 'height unchanged');

done_testing;
