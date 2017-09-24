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
# Regression test: New windows were attached to the container of a floating window
# if only a floating window is present on the workspace.

use i3test;

my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

#############################################################################
# 1: open a floating window, get it mapped
#############################################################################

# Create a floating window
my $window = open_floating_window;
ok($window->mapped, 'Window is mapped');

my $ws = get_ws($tmp);
my ($nodes, $focus) = get_ws_content($tmp);

is(@{$ws->{floating_nodes}}, 1, 'one floating node');
is(@{$nodes}, 0, 'no tiling nodes');

# Create a tiling window
my $twindow = open_window;

($nodes, $focus) = get_ws_content($tmp);

is(@{$nodes}, 1, 'one tiling node');

#############################################################################
# 2: similar case: floating windows should be attached at the currently focused
# position in the workspace (for example a stack), not just at workspace level.
#############################################################################

$tmp = fresh_workspace;

my $first = open_window;
my $second = open_window;

cmd 'layout stacked';

$ws = get_ws($tmp);
is(@{$ws->{floating_nodes}}, 0, 'no floating nodes so far');
is(@{$ws->{nodes}}, 1, 'one tiling node (stacked con)');

# Create a floating window
$window = open_floating_window;
ok($window->mapped, 'Window is mapped');

$ws = get_ws($tmp);
is(@{$ws->{floating_nodes}}, 1, 'one floating nodes');
is(@{$ws->{nodes}}, 1, 'one tiling node (stacked con)');

my $third = open_window;


$ws = get_ws($tmp);
is(@{$ws->{floating_nodes}}, 1, 'one floating nodes');
is(@{$ws->{nodes}}, 1, 'one tiling node (stacked con)');

done_testing;
