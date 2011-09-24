#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression test for inplace restarting with dock clients
#
use X11::XCB qw(:all);
use i3test;

BEGIN {
    use_ok('X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;
my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

#####################################################################
# verify that there is no dock window yet
#####################################################################

# Children of all dockareas
my @docked = get_dock_clients;

is(@docked, 0, 'no dock clients yet');

# open a dock client

my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#FF0000',
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    event_mask => [ 'structure_notify' ],
);

$window->map;

wait_for_map $x;

#####################################################################
# check that we can find it in the layout tree at the expected position
#####################################################################

@docked = get_dock_clients;
is(@docked, 1, 'one dock client found');

# verify the height
my $docknode = $docked[0];

is($docknode->{rect}->{height}, 30, 'dock node has unchanged height');

# perform an inplace-restart
cmd 'restart';

sleep 0.25;

does_i3_live;


#####################################################################
# check that we can still find the dock client
#####################################################################

@docked = get_dock_clients;
is(@docked, 1, 'one dock client found');
$docknode = $docked[0];

is($docknode->{rect}->{height}, 30, 'dock node has unchanged height after restart');

$window->destroy;

wait_for_unmap $x;

@docked = get_dock_clients;
is(@docked, 0, 'no dock clients found');

#####################################################################
# create a dock client with a 1px border
#####################################################################

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    border => 1,
    rect => [ 0, 0, 30, 20],
    background_color => '#00FF00',
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    event_mask => [ 'structure_notify' ],
);

$window->map;

wait_for_map $x;

@docked = get_dock_clients;
is(@docked, 1, 'one dock client found');
$docknode = $docked[0];

is($docknode->{rect}->{height}, 20, 'dock node has unchanged height');

cmd 'restart';
sleep 0.25;

@docked = get_dock_clients;
is(@docked, 1, 'one dock client found');
$docknode = $docked[0];

is($docknode->{rect}->{height}, 20, 'dock node has unchanged height');


done_testing;
