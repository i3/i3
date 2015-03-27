#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)

use i3test;
use X11::XCB 'PROP_MODE_REPLACE';
use List::Util qw(first);

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
my $window = open_window({
        window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    });

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

#####################################################################
# check that re-configuring the height works
#####################################################################

$window->rect(X11::XCB::Rect->new(x => 0, y => 0, width => 50, height => 40));

sync_with_i3;

@docked = get_dock_clients('top');
is(@docked, 1, 'one dock client found');

# verify the position/size
$docknode = $docked[0];

is($docknode->{rect}->{x}, 0, 'dock node placed at x=0');
is($docknode->{rect}->{y}, 0, 'dock node placed at y=0');
is($docknode->{rect}->{width}, $primary->rect->width, 'dock node as wide as the screen');
is($docknode->{rect}->{height}, 40, 'dock height changed');

$window->destroy;

wait_for_unmap $window;

@docked = get_dock_clients();
is(@docked, 0, 'no more dock clients');

#####################################################################
# check if it gets placed on bottom (by coordinates)
#####################################################################

$window = open_window({
        rect => [ 0, 1000, 30, 30 ],
        background_color => '#FF0000',
        window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    });

$rect = $window->rect;
is($rect->width, $primary->rect->width, 'dock client is as wide as the screen');
is($rect->height, 30, 'height is unchanged');

@docked = get_dock_clients('bottom');
is(@docked, 1, 'dock client on bottom');

$window->destroy;

wait_for_unmap $window;

@docked = get_dock_clients();
is(@docked, 0, 'no more dock clients');

#####################################################################
# check if it gets placed on bottom (by hint)
#####################################################################

$window = open_window({
        dont_map => 1,
        rect => [ 0, 1000, 30, 30 ],
        background_color => '#FF0000',
        window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    });

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

wait_for_map $window;

@docked = get_dock_clients('top');
is(@docked, 1, 'dock client on top');

# now change strut_partial to reserve space on the bottom and the dock should
# be moved to the bottom dock area
$x->change_property(
    PROP_MODE_REPLACE,
    $window->id,
    $atomname->id,
    $atomtype->id,
    32,         # 32 bit integer
    12,
    pack('L12', 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 1280, 0)
);

sync_with_i3;
@docked = get_dock_clients('bottom');
is(@docked, 1, 'dock client on bottom');

$window->destroy;

wait_for_unmap $window;

@docked = get_dock_clients();
is(@docked, 0, 'no more dock clients');

$window = open_window({
        dont_map => 1,
        rect => [ 0, 1000, 30, 30 ],
        background_color => '#FF0000',
        window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    });

$window->_create();

# Add a _NET_WM_STRUT_PARTIAL hint
$atomname = $x->atom(name => '_NET_WM_STRUT_PARTIAL');
$atomtype = $x->atom(name => 'CARDINAL');

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

wait_for_map $window;

@docked = get_dock_clients('bottom');
is(@docked, 1, 'dock client on bottom');

$window->destroy;


#####################################################################
# regression test: transient dock client
#####################################################################

my $fwindow = open_window({
        dont_map => 1,
        background_color => '#FF0000',
        window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    });

$fwindow->transient_for($window);
$fwindow->map;

wait_for_map $fwindow;

does_i3_live;

done_testing;
