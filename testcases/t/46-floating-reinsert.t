#!perl
# vim:ts=4:sw=4:expandtab
#
use X11::XCB qw(:all);
use Time::HiRes qw(sleep);
use i3test tests => 5;

BEGIN {
    use_ok('X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;

my $tmp = get_unused_workspace;
cmd "workspace $tmp";

my $left = open_standard_window($x);
sleep 0.25;
my $mid = open_standard_window($x);
sleep 0.25;

cmd 'split v';
my $bottom = open_standard_window($x);
sleep 0.25;

my ($nodes, $focus) = get_ws_content($tmp);

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

($nodes, $focus) = get_ws_content($tmp);
is(@{$nodes->[1]->{nodes}}, 2, 'two windows in split con');

#############################################################################
# 2: make it tiling, see where it ends up
#############################################################################

cmd 'mode toggle';

my ($nodes, $focus) = get_ws_content($tmp);

is(@{$nodes->[1]->{nodes}}, 3, 'three windows in split con after mode toggle');
