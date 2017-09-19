package i3test::Test;
# vim:ts=4:sw=4:expandtab

use base 'Test::Builder::Module';

our @EXPORT = qw(
    is_num_children
    is_num_fullscreen
    cmp_float
    does_i3_live
);

my $CLASS = __PACKAGE__;

=head1 NAME

i3test::Test - Additional test instructions for use in i3 testcases

=head1 SYNOPSIS

  use i3test;

  my $ws = fresh_workspace;
  is_num_children($ws, 0, 'no containers on this workspace yet');
  cmd 'open';
  is_num_children($ws, 1, 'one container after "open"');

  done_testing;

=head1 DESCRIPTION

This module provides convenience methods for i3 testcases. If you notice that a
certain pattern is present in 5 or more test cases, it should most likely be
moved into this module.

=head1 EXPORT

=head2 is_num_children($workspace, $expected, $test_name)

Gets the number of children on the given workspace and verifies that they match
the expected amount of children.

  is_num_children('1', 0, 'no containers on workspace 1 at startup');

=cut

sub is_num_children {
    my ($workspace, $num_children, $name) = @_;
    my $tb = $CLASS->builder;

    my $con = i3test::get_ws($workspace);
    $tb->ok(defined($con), "Workspace $workspace exists");
    if (!defined($con)) {
        $tb->skip("Workspace does not exist, skipping is_num_children");
        return;
    }

    my $got_num_children = scalar @{$con->{nodes}};

    $tb->is_num($got_num_children, $num_children, $name);
}

=head2 is_num_fullscreen($workspace, $expected, $test_name)

Gets the number of fullscreen containers on the given workspace and verifies that
they match the expected amount.

  is_num_fullscreen('1', 0, 'no fullscreen containers on workspace 1');

=cut
sub is_num_fullscreen {
    my ($workspace, $num_fullscreen, $name) = @_;
    my $workspace_content = i3test::get_ws($workspace);
    my $tb = $CLASS->builder;

    my $nodes = scalar grep { $_->{fullscreen_mode} != 0 } @{$workspace_content->{nodes}->[0]->{nodes}};
    my $cons = scalar grep { $_->{fullscreen_mode} != 0 } @{$workspace_content->{nodes}};
    my $floating = scalar grep { $_->{fullscreen_mode} != 0 } @{$workspace_content->{floating_nodes}->[0]->{nodes}};
    $tb->is_num($nodes + $cons + $floating, $num_fullscreen, $name);
}

=head2 cmp_float($a, $b)

Compares floating point numbers C<$a> and C<$b> and returns true if they differ
less then 1e-6.

  $tmp = fresh_workspace;

  open_window for (1..4);

  cmd 'resize grow width 10 px or 25 ppt';

  ($nodes, $focus) = get_ws_content($tmp);
  ok(cmp_float($nodes->[0]->{percent}, 0.166666666666667), 'first window got 16%');
  ok(cmp_float($nodes->[1]->{percent}, 0.166666666666667), 'second window got 16%');
  ok(cmp_float($nodes->[2]->{percent}, 0.166666666666667), 'third window got 16%');
  ok(cmp_float($nodes->[3]->{percent}, 0.50), 'fourth window got 50%');

=cut
sub cmp_float {
  my ($a, $b, $name) = @_;
  my $tb = $CLASS->builder;

  $tb->cmp_ok(abs($a - $b), '<', 1e-6, $name);
}

=head2 does_i3_live

Returns true if the layout tree can still be received from i3.

  # i3 used to crash on invalid commands in revision X
  cmd 'invalid command';
  does_i3_live;

=cut
sub does_i3_live {
    my $tree = i3test::i3(i3test::get_socket_path())->get_tree->recv;
    my @nodes = @{$tree->{nodes}};
    my $tb = $CLASS->builder;
    $tb->ok((@nodes > 0), 'i3 still lives');
}

=head1 AUTHOR

Michael Stapelberg <michael@i3wm.org>

=cut

1
