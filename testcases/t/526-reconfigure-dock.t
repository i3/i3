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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Test reconfiguration of dock clients.
# Ticket: #1883
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0

bar {
    # Disable i3bar, which is also a dock client.
    i3bar_command :
}
EOT

my ($window, $rect);
my (@docks);

###############################################################################
# 1: Given two screens A and B and a dock client on screen A, when the dock
#    client is reconfigured to be positioned on screen B, then the client is
#    moved to the correct position.
###############################################################################

$window = open_window({
        window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK')
    });

$rect = $window->rect;
is($rect->x, 0, 'sanity check: dock client is on the left screen');

$window->rect(X11::XCB::Rect->new(x => 1024, y => 0, width => 1024, height => 30));
sync_with_i3;

@docks = get_dock_clients;
is(@docks, 1, 'there is still exactly one dock');

is($docks[0]->{rect}->{x}, 1024, 'dock client has moved to the other screen');

###############################################################################

done_testing;
