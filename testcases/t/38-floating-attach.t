#!perl
# vim:ts=4:sw=4:expandtab
# Regression test: New windows were attached to the container of a floating window
# if only a floating window is present on the workspace.

use i3test;
use X11::XCB qw(:all);

BEGIN {
    use_ok('X11::XCB::Window');
}

my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

#############################################################################
# 1: open a floating window, get it mapped
#############################################################################

my $x = X11::XCB::Connection->new;

# Create a floating window
my $window = open_standard_window($x, undef, 1);
ok($window->mapped, 'Window is mapped');

my $ws = get_ws($tmp);
my ($nodes, $focus) = get_ws_content($tmp);

is(@{$ws->{floating_nodes}}, 1, 'one floating node');
is(@{$nodes}, 0, 'no tiling nodes');

# Create a tiling window
my $twindow = open_standard_window($x);

($nodes, $focus) = get_ws_content($tmp);

is(@{$nodes}, 1, 'one tiling node');

#############################################################################
# 2: similar case: floating windows should be attached at the currently focused
# position in the workspace (for example a stack), not just at workspace level.
#############################################################################

$tmp = fresh_workspace;

my $first = open_standard_window($x);
my $second = open_standard_window($x);

cmd 'layout stacked';

$ws = get_ws($tmp);
is(@{$ws->{floating_nodes}}, 0, 'no floating nodes so far');
is(@{$ws->{nodes}}, 1, 'one tiling node (stacked con)');

# Create a floating window
my $window = open_standard_window($x, undef, 1);
ok($window->mapped, 'Window is mapped');

$ws = get_ws($tmp);
is(@{$ws->{floating_nodes}}, 1, 'one floating nodes');
is(@{$ws->{nodes}}, 1, 'one tiling node (stacked con)');

my $third = open_standard_window($x);


$ws = get_ws($tmp);
is(@{$ws->{floating_nodes}}, 1, 'one floating nodes');
is(@{$ws->{nodes}}, 1, 'one tiling node (stacked con)');

done_testing;
