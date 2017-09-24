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
# by moving the window in the opposite orientation that its parent has, we
# force i3 to create a new split container with the appropriate orientation.
# However, when doing that two times in a row, we end up with two split
# containers which are then redundant (workspace is horizontal, then v-split,
# then h-split – we could just append the children of the latest h-split to the
# workspace itself).
#
# This testcase checks that the tree is properly flattened after moving.
#
use i3test;

my $tmp = fresh_workspace;

my $left = open_window;
my $mid = open_window;
my $right = open_window;

cmd 'move up';
cmd 'move right';
my $ws = get_ws($tmp);

is($ws->{layout}, 'splith', 'workspace layout is splith');
is(@{$ws->{nodes}}, 3, 'all three windows on workspace level');

################################################################################
# Ticket #1053 provides a sequence of operations where the flattening does not
# work correctly:
################################################################################

$tmp = fresh_workspace;

my $tab1 = open_window;
my $tab2 = open_window;
$mid = open_window;
$right = open_window;
cmd 'focus right';
cmd 'split v';
cmd 'focus right';
cmd 'move left';
cmd 'layout tabbed';
cmd 'focus parent';
cmd 'split v';

$ws = get_ws($tmp);
my @nodes = @{$ws->{nodes}};
is(@nodes, 3, 'all three windows on workspace level');
is($nodes[0]->{layout}, 'splitv', 'first node is splitv');
is(@{$nodes[0]->{nodes}}, 1, 'one node in the first node');
is($nodes[0]->{nodes}->[0]->{layout}, 'tabbed', 'tabbed layout');
is(@{$nodes[0]->{nodes}->[0]->{nodes}}, 2, 'two nodes in that node');

cmd 'focus right';
cmd 'move left';

$ws = get_ws($tmp);
@nodes = @{$ws->{nodes}};
is(@nodes, 2, 'all three windows on workspace level');

done_testing;
