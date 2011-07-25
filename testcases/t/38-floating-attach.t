#!perl
# vim:ts=4:sw=4:expandtab
# Regression test: New windows were attached to the container of a floating window
# if only a floating window is present on the workspace.

use i3test;
use X11::XCB qw(:all);
use Time::HiRes qw(sleep);

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
my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#C0C0C0',
    # replace the type with 'utility' as soon as the coercion works again in X11::XCB
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
);

isa_ok($window, 'X11::XCB::Window');

$window->map;

sleep 0.25;

ok($window->mapped, 'Window is mapped');

my $ws = get_ws($tmp);
my ($nodes, $focus) = get_ws_content($tmp);

is(@{$ws->{floating_nodes}}, 1, 'one floating node');
is(@{$nodes}, 0, 'no tiling nodes');

# Create a tiling window
my $twindow = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#C0C0C0',
);

isa_ok($twindow, 'X11::XCB::Window');

$twindow->map;

sleep 0.25;

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
my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#C0C0C0',
    # replace the type with 'utility' as soon as the coercion works again in X11::XCB
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
);

isa_ok($window, 'X11::XCB::Window');

$window->map;

sleep 0.25;

ok($window->mapped, 'Window is mapped');

$ws = get_ws($tmp);
is(@{$ws->{floating_nodes}}, 1, 'one floating nodes');
is(@{$ws->{nodes}}, 1, 'one tiling node (stacked con)');

my $third = open_standard_window($x);


$ws = get_ws($tmp);
is(@{$ws->{floating_nodes}}, 1, 'one floating nodes');
is(@{$ws->{nodes}}, 1, 'one tiling node (stacked con)');

done_testing;
