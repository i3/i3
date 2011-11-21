#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression test for inplace restarting with dock clients
#
use i3test;

my $tmp = fresh_workspace;

#####################################################################
# verify that there is no dock window yet
#####################################################################

# Children of all dockareas
my @docked = get_dock_clients;

is(@docked, 0, 'no dock clients yet');

# open a dock client

my $window = open_window($x, {
        background_color => '#FF0000',
        window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    });

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

wait_for_unmap $window;

@docked = get_dock_clients;
is(@docked, 0, 'no dock clients found');

#####################################################################
# create a dock client with a 1px border
#####################################################################

$window = open_window($x, {
        border => 1,
        rect => [ 0, 0, 30, 20 ],
        background_color => '#00FF00',
        window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    });

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
