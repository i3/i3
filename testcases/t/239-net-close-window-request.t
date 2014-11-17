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
# Test _NET_CLOSE_WINDOW requests to close a window.
# See http://standards.freedesktop.org/wm-spec/wm-spec-latest.html#idm140200472668896
# Ticket: #1396
# Bug still in: 4.8-116-gbb1f857
use i3test;

sub send_close_window_request {
    my ($win) = @_;

    my $msg = pack "CCSLLLLLL",
        X11::XCB::CLIENT_MESSAGE, # response_type
        32, # format
        0, # sequence
        $win->id, # window
        $x->atom(name => '_NET_CLOSE_WINDOW')->id, # message type
        0, # data32[0]
        0, # data32[1]
        0, # data32[2]
        0, # data32[3]
        0; # data32[4]

    $x->send_event(0, $x->get_root_window(), X11::XCB::EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);
}

my $ws = fresh_workspace;
my $win = open_window;

send_close_window_request($win);
sync_with_i3;

is(@{get_ws($ws)->{nodes}}, 0, 'When a pager sends a _NET_CLOSE_WINDOW request for a window, the container should be closed');

done_testing;
