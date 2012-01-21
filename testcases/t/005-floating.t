#!perl
# vim:ts=4:sw=4:expandtab

use i3test;
use X11::XCB qw(:all);

BEGIN {
    use_ok('X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;

# Create a floating window which is smaller than the minimum enforced size of i3
my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#C0C0C0',
    # replace the type with 'utility' as soon as the coercion works again in X11::XCB
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
    event_mask => [ 'structure_notify' ],
);

isa_ok($window, 'X11::XCB::Window');

$window->map;

wait_for_map $x;

my ($absolute, $top) = $window->rect;

ok($window->mapped, 'Window is mapped');
cmp_ok($absolute->{width}, '>=', 75, 'i3 raised the width to 75');
cmp_ok($absolute->{height}, '>=', 50, 'i3 raised the height to 50');

ok($absolute->{x} != 0 && $absolute->{y} != 0, 'i3 did not map it to (0x0)');

$window->unmap;

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 20, 20, 80, 90],
    background_color => '#C0C0C0',
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
    event_mask => [ 'structure_notify' ],
);

isa_ok($window, 'X11::XCB::Window');

$window->map;

wait_for_map $x;

($absolute, $top) = $window->rect;

cmp_ok($absolute->{width}, '==', 80, "i3 let the width at 80");
cmp_ok($absolute->{height}, '==', 90, "i3 let the height at 90");

cmp_ok($top->{x}, '==', 20, 'i3 mapped it to x=20');
cmp_ok($top->{y}, '==', 20, 'i3 mapped it to y=20');

$window->unmap;

#####################################################################
# check that a tiling window which is then made floating still has
# at least the size of its initial geometry
#####################################################################

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 1, 1, 80, 90],
    background_color => '#C0C0C0',
    #window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
    event_mask => [ 'structure_notify' ],
);

isa_ok($window, 'X11::XCB::Window');

$window->map;

wait_for_map $x;

cmd 'floating enable';

($absolute, $top) = $window->rect;

cmp_ok($absolute->{width}, '==', 80, "i3 let the width at 80");
cmp_ok($absolute->{height}, '==', 90, "i3 let the height at 90");

$window->unmap;

done_testing;
