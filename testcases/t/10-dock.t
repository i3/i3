#!perl
# vim:ts=4:sw=4:expandtab

use i3test;
use X11::XCB qw(:all);
use Time::HiRes qw(sleep);
use List::Util qw(first);

BEGIN {
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

my $x = X11::XCB::Connection->new;
my $i3 = i3("/tmp/nestedcons");

#####################################################################
# verify that there is no dock window yet
#####################################################################

# Children of all dockareas
my @docked = get_dock_clients;
is(@docked, 0, 'no dock clients yet');

#####################################################################
# Create a dock window and see if it gets managed
#####################################################################

my $screens = $x->screens;

# Get the primary screen
my $primary = first { $_->primary } @{$screens};

# TODO: focus the primary screen before

my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#FF0000',
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
);

$window->map;

sleep 0.25;

my $rect = $window->rect;
is($rect->width, $primary->rect->width, 'dock client is as wide as the screen');
is($rect->height, 30, 'height is unchanged');

#####################################################################
# check that we can find it in the layout tree at the expected position
#####################################################################

@docked = get_dock_clients('top');
is(@docked, 1, 'one dock client found');

# verify the position/size
my $docknode = $docked[0];

is($docknode->{rect}->{x}, 0, 'dock node placed at x=0');
is($docknode->{rect}->{y}, 0, 'dock node placed at y=0');
is($docknode->{rect}->{width}, $primary->rect->width, 'dock node as wide as the screen');
is($docknode->{rect}->{height}, 30, 'dock node has unchanged height');

$window->destroy;

sleep 0.25;

@docked = get_dock_clients();
is(@docked, 0, 'no more dock clients');

#####################################################################
# check if it gets placed on bottom (by coordinates)
#####################################################################

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 1000, 30, 30],
    background_color => '#FF0000',
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
);

$window->map;

sleep 0.25;

my $rect = $window->rect;
is($rect->width, $primary->rect->width, 'dock client is as wide as the screen');
is($rect->height, 30, 'height is unchanged');

@docked = get_dock_clients('bottom');
is(@docked, 1, 'dock client on bottom');

$window->destroy;

sleep 0.25;

@docked = get_dock_clients();
is(@docked, 0, 'no more dock clients');

#####################################################################
# check if it gets placed on bottom (by hint)
#####################################################################

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 1000, 30, 30],
    background_color => '#FF0000',
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
);

$window->_create();

# Add a _NET_WM_STRUT_PARTIAL hint
my $atomname = $x->atom(name => '_NET_WM_STRUT_PARTIAL');
my $atomtype = $x->atom(name => 'CARDINAL');

$x->change_property(
    PROP_MODE_REPLACE,
    $window->id,
    $atomname->id,
    $atomtype->id,
    32,         # 32 bit integer
    12,
    pack('L12', 0, 0, 16, 0, 0, 0, 0, 0, 0, 1280, 0, 0)
);

$window->map;

sleep 0.25;

@docked = get_dock_clients('top');
is(@docked, 1, 'dock client on top');

$window->destroy;

sleep 0.25;

@docked = get_dock_clients();
is(@docked, 0, 'no more dock clients');

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 1000, 30, 30],
    background_color => '#FF0000',
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
);

$window->_create();

# Add a _NET_WM_STRUT_PARTIAL hint
my $atomname = $x->atom(name => '_NET_WM_STRUT_PARTIAL');
my $atomtype = $x->atom(name => 'CARDINAL');

$x->change_property(
    PROP_MODE_REPLACE,
    $window->id,
    $atomname->id,
    $atomtype->id,
    32,         # 32 bit integer
    12,
    pack('L12', 0, 0, 0, 16, 0, 0, 0, 0, 0, 1280, 0, 0)
);

$window->map;

sleep 0.25;

@docked = get_dock_clients('bottom');
is(@docked, 1, 'dock client on bottom');

$window->destroy;


#####################################################################
# regression test: transient dock client
#####################################################################

my $fwindow = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#FF0000',
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
);

$fwindow->transient_for($window);
$fwindow->map;

sleep 0.25;

does_i3_live;

done_testing;
