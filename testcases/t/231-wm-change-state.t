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
# Correctly handle WM_CHANGE_STATE requests for the iconic state
# See http://tronche.com/gui/x/icccm/sec-4.html#s-4.1.4
# Ticket: #1279
# Bug still in: 4.8-7-gf4a8253
use i3test;

sub send_iconic_state_request {
    my ($win) = @_;

    my $msg = pack "CCSLLLLLL",
        X11::XCB::CLIENT_MESSAGE, # response_type
        32, # format
        0, # sequence
        $win->id, # window
        $x->atom(name => 'WM_CHANGE_STATE')->id, # message type
        3, # data32[0]
        0, # data32[1]
        0, # data32[2]
        0, # data32[3]
        0; # data32[4]

    $x->send_event(0, $x->get_root_window(), X11::XCB::EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);
}

my $ws = fresh_workspace;
my $win = open_window;

send_iconic_state_request($win);
sync_with_i3;

is(@{get_ws($ws)->{nodes}}, 0, 'When a window requests the iconic state, the container should be closed');

done_testing;
