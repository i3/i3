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
# Verify that _NET_WM_DESKTOP sticky requests do not conflict with dock
# clients, resulting in a crash
# Ticket: #4039
# Bug still in: 4.18-238-g4d55bba7f
use i3test;

sub send_sticky_request {
    my ($win) = @_;

    my $msg = pack "CCSLLLLLL",
        X11::XCB::CLIENT_MESSAGE, # response_type
        32, # format
        0, # sequence
        $win->id, # window
        $x->atom(name => '_NET_WM_DESKTOP')->id, # message type
        hex '0xFFFFFFFF', # data32[0] = NET_WM_DESKTOP_ALL
        0, # data32[1]
        0, # data32[2]
        0, # data32[3]
        0; # data32[4]

    $x->send_event(0, $x->get_root_window(), X11::XCB::EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);
}

# Test the normal functionality first
my $ws = fresh_workspace;
my $win = open_window;

is(@{get_ws($ws)->{floating_nodes}}, 0, 'No floating windows yet');
send_sticky_request($win);
sync_with_i3;

is(@{get_ws($ws)->{floating_nodes}}, 1, 'One floating (sticky) window');
$ws = fresh_workspace;
is(@{get_ws($ws)->{floating_nodes}}, 1, 'Sticky window in new workspace');

# See #4039
kill_all_windows;
$win = open_window({
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK')
});

send_sticky_request($win);
sync_with_i3;
is(@{get_ws($ws)->{floating_nodes}}, 0, 'Dock client did not get sticky/floating');

# Cause a ConfigureRequest by setting the window’s position/size.
my ($a, $t) = $win->rect;
$win->rect(X11::XCB::Rect->new(x => 0, y => 0, width => $a->width, height => $a->height));

does_i3_live;
is(@{get_ws($ws)->{floating_nodes}}, 0, 'Dock client still not sticky/floating');

done_testing;
