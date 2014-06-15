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
#
# Test that _NET_CLIENT_LIST is properly updated on the root window as windows
# are mapped and unmapped.
#
# Information on this property can be found here:
# http://standards.freedesktop.org/wm-spec/latest/ar01s03.html
#
# > These arrays contain all X Windows managed by the Window Manager.
# > _NET_CLIENT_LIST has initial mapping order, starting with the oldest window.
#
# Ticket: #1099
# Bug still in: 4.7.2-8-ge6cce92
use i3test;

sub get_client_list {
    my $cookie = $x->get_property(
        0,
        $x->get_root_window(),
        $x->atom(name => '_NET_CLIENT_LIST')->id,
        $x->atom(name => 'WINDOW')->id,
        0,
        4096,
    );
    my $reply = $x->get_property_reply($cookie->{sequence});
    my $len = $reply->{length};

    return () if $len == 0;
    return unpack("L$len", $reply->{value});
}

# Mapping a window should give us one client in _NET_CLIENT_LIST
my $win1 = open_window;

my @clients = get_client_list;

is(@clients, 1, 'One client in _NET_CLIENT_LIST');
is($clients[0], $win1->{id}, 'Correct client in position one');

# Mapping another window should give us two clients in the list with the last
# client mapped in the last position
my $win2 = open_window;

@clients = get_client_list;
is(@clients, 2, 'Added mapped client to list (2)');
is($clients[0], $win1->{id}, 'Correct client in position one');
is($clients[1], $win2->{id}, 'Correct client in position two');

# Mapping another window should give us three clients in the list in the order
# they were mapped
my $win3 = open_window;

@clients = get_client_list;
is(@clients, 3, 'Added mapped client to list (3)');
is($clients[0], $win1->{id}, 'Correct client in position one');
is($clients[1], $win2->{id}, 'Correct client in position two');
is($clients[2], $win3->{id}, 'Correct client in position three');

# Unmapping the second window should give us the two remaining clients in the
# order they were mapped
$win2->unmap;
wait_for_unmap($win2);

@clients = get_client_list;
is(@clients, 2, 'Removed unmapped client from list (2)');
is($clients[0], $win1->{id}, 'Correct client in position one');
is($clients[1], $win3->{id}, 'Correct client in position two');

# Unmapping the first window should give us only the remaining mapped window in
# the list
$win1->unmap;
wait_for_unmap($win1);

@clients = get_client_list;
is(@clients, 1, 'Removed unmapped client from list (1)');
is($clients[0], $win3->{id}, 'Correct client in position one');

# Unmapping the last window should give us an empty list
$win3->unmap;
wait_for_unmap($win3);

@clients = get_client_list;
is(@clients, 0, 'Removed unmapped client from list (0)');

# Dock clients should not be included in this list

my $dock_win = open_window({
        window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    });

@clients = get_client_list;
is(@clients, 0, 'Dock clients are not included in the list');

done_testing;
