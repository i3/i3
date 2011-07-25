#!perl
# vim:ts=4:sw=4:expandtab

use i3test;
use X11::XCB qw(:all);

BEGIN {
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

my $x = X11::XCB::Connection->new;
my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

####################################################################################
# first part: test if a floating window will be correctly positioned above its leader
#
# This is verified by opening two windows, then opening a floating window above the
# right one, then above the left one. If the floating windows are all positioned alike,
# one of both (depending on your screen resolution) will be positioned wrong.
####################################################################################

my $left = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [0, 0, 30, 30],
    background_color => '#FF0000',
);

$left->name('Left');
$left->map;

my $right = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [0, 0, 30, 30],
    background_color => '#FF0000',
);

$right->name('Right');
$right->map;

sleep 0.25;

my ($abs, $rgeom) = $right->rect;

my $child = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#C0C0C0',
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
);

$child->name('Child window');
$child->client_leader($right);
$child->map;

sleep 0.25;

my $cgeom;
($abs, $cgeom) = $child->rect;
cmp_ok($cgeom->x, '>=', $rgeom->x, 'Child X >= right container X');

my $child2 = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#C0C0C0',
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
);

$child2->name('Child window 2');
$child2->client_leader($left);
$child2->map;

sleep 0.25;

($abs, $cgeom) = $child2->rect;
cmp_ok(($cgeom->x + $cgeom->width), '<', $rgeom->x, 'child above left window');

# check wm_transient_for


my $fwindow = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#FF0000',
);

$fwindow->transient_for($right);
$fwindow->map;

sleep 0.25;

my ($absolute, $top) = $fwindow->rect;
ok($absolute->{x} != 0 && $absolute->{y} != 0, 'i3 did not map it to (0x0)');

SKIP: {
    skip "(workspace placement by client_leader not yet implemented)", 3;

#####################################################################
# Create a parent window
#####################################################################

my $window = $x->root->create_child(
class => WINDOW_CLASS_INPUT_OUTPUT,
rect => [ 0, 0, 30, 30 ],
background_color => '#C0C0C0',
);

$window->name('Parent window');
$window->map;

sleep 0.25;

#########################################################################
# Switch to a different workspace and open a child window. It should be opened
# on the old workspace.
#########################################################################
fresh_workspace;

my $child = $x->root->create_child(
class => WINDOW_CLASS_INPUT_OUTPUT,
rect => [ 0, 0, 30, 30 ],
background_color => '#C0C0C0',
);

$child->name('Child window');
$child->client_leader($window);
$child->map;

sleep 0.25;

isnt($x->input_focus, $child->id, "Child window focused");

# Switch back
cmd "workspace $tmp";

is($x->input_focus, $child->id, "Child window focused");

}

done_testing;
