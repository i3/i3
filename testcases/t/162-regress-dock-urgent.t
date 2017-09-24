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
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Regression test for setting the urgent hint on dock clients.
# found in 4be3178d4d360c2996217d811e61161c84d25898
#
use i3test;

my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

#####################################################################
# verify that there is no dock window yet
#####################################################################

# Children of all dockareas
my @docked = get_dock_clients;

is(@docked, 0, 'no dock clients yet');

# open a dock client

my $window = open_window(
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
);

#####################################################################
# check that we can find it in the layout tree at the expected position
#####################################################################

@docked = get_dock_clients;
is(@docked, 1, 'one dock client found');

# verify the height
my $docknode = $docked[0];

is($docknode->{rect}->{height}, 30, 'dock node has unchanged height');

$window->add_hint('urgency');

sync_with_i3;

does_i3_live;

done_testing;
