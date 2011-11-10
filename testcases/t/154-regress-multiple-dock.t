#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression test for closing one of multiple dock clients
#
use X11::XCB qw(:all);
use i3test;

BEGIN {
    use_ok('X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;

my $tmp = fresh_workspace;

#####################################################################
# verify that there is no dock window yet
#####################################################################

# Children of all dockareas
my @docked = get_dock_clients;

is(@docked, 0, 'no dock clients yet');

#####################################################################
# open a dock client
#####################################################################

my $first = open_window($x, {
        background_color => '#FF0000',
        window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    });

#####################################################################
# Open a second dock client
#####################################################################

my $second = open_window($x, {
        background_color => '#FF0000',
        window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    });

#####################################################################
# Kill the second dock client
#####################################################################
cmd "nop destroying dock client";
$second->destroy;

#####################################################################
# Now issue a focus command
#####################################################################
cmd 'focus right';

does_i3_live;

done_testing;
