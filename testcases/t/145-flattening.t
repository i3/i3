#!perl
# vim:ts=4:sw=4:expandtab
#
# by moving the window in the opposite orientation that its parent has, we
# force i3 to create a new split container with the appropriate orientation.
# However, when doing that two times in a row, we end up with two split
# containers which are then redundant (workspace is horizontal, then v-split,
# then h-split – we could just append the children of the latest h-split to the
# workspace itself).
#
# This testcase checks that the tree is properly flattened after moving.
#
use X11::XCB qw(:all);
use i3test;

my $x = X11::XCB::Connection->new;

my $tmp = fresh_workspace;

my $left = open_window($x);
my $mid = open_window($x);
my $right = open_window($x);

cmd 'move up';
cmd 'move right';
my $ws = get_ws($tmp);

is($ws->{orientation}, 'horizontal', 'workspace orientation is horizontal');
is(@{$ws->{nodes}}, 3, 'all three windows on workspace level');

done_testing;
