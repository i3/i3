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
# Test that i3 supports setting multiple _NET_WM_STATE changes in one ClientMessage.
# Bug still in: 4.23-21-g6a530de2
use i3test;

my $_NET_WM_STATE_REMOVE = 0;
my $_NET_WM_STATE_ADD = 1;
my $_NET_WM_STATE_TOGGLE = 2;
sub send_event {
    my ($win, $add) = @_;
    my $msg = pack "CCSLLLLLL",
        X11::XCB::CLIENT_MESSAGE, # response_type
        32, # format
        0, # sequence
        $win->id, # window
        $x->atom(name => '_NET_WM_STATE')->id, # message type
        ($add ? $_NET_WM_STATE_ADD : $_NET_WM_STATE_REMOVE), # data32[0]
        $x->atom(name => '_NET_WM_STATE_FULLSCREEN')->id, # data32[1]
        $x->atom(name => '_NET_WM_STATE_STICKY')->id, # data32[2]
        0, # data32[3]
        0; # data32[4]

    $x->send_event(0, $x->get_root_window(), X11::XCB::EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);
    sync_with_i3;
}

my $ws1 = fresh_workspace;
my $win = open_floating_window;

# Nothing to remove
send_event($win, 0);
is_num_fullscreen($ws1, 0, 'no fullscreen window');

# Enable
send_event($win, 1);
is_num_fullscreen($ws1, 1, 'one fullscreen window');
my $ws2 = fresh_workspace;
is_num_fullscreen($ws2, 1, 'sticky fullscreen window in second workspace');

# Disable
send_event($win, 0);
is_num_fullscreen($ws1, 0, 'no fullscreen windows');
is_num_fullscreen($ws2, 0, 'no fullscreen windows');
cmd "workspace $ws1";
is(@{get_ws($ws1)->{floating_nodes}}, 0, 'No floating (sticky) window in first workspace');
is(@{get_ws($ws2)->{floating_nodes}}, 1, 'One floating (non-sticky) window in second workspace');

done_testing;
