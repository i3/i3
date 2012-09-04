package i3test::Test;

use base 'Test::Builder::Module';

our @EXPORT = qw(is_num_children);

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

=head1 AUTHOR

Michael Stapelberg <michael@i3wm.org>

=cut

1
