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
# Tests if the WM_TAKE_FOCUS protocol is correctly handled by i3
#
# For more information on the protocol and input handling, see:
# http://tronche.com/gui/x/icccm/sec-4.html#s-4.1.7
#
use i3test;

sub recv_take_focus {
    my ($window) = @_;

    # sync_with_i3 will send a ClientMessage to i3 and i3 will send the same
    # payload back to $window->id.
    my $myrnd = sync_with_i3(
        window_id => $window->id,
        dont_wait_for_event => 1,
    );

    # We check whether the first received message has the correct payload — if
    # not, the received message was a WM_TAKE_FOCUS message.
    my $first_event_is_clientmessage;
    wait_for_event 2, sub {
        my ($event) = @_;
        # TODO: const
        return 0 unless $event->{response_type} == 161;

        my ($win, $rnd) = unpack "LL", $event->{data};
        if (!defined($first_event_is_clientmessage)) {
            $first_event_is_clientmessage = ($rnd == $myrnd);
        }
        return ($rnd == $myrnd);
    };

    return !$first_event_is_clientmessage;
}

subtest 'Window without WM_TAKE_FOCUS', sub {
    fresh_workspace;

    my $window = open_window;

    ok(!recv_take_focus($window), 'did not receive ClientMessage');

    done_testing;
};

# http://tronche.com/gui/x/icccm/sec-4.html#s-4.1.7
# > Clients using the Globally Active model can only use a SetInputFocus request
# > to acquire the input focus when they do not already have it on receipt of one
# > of the following events:
# > * ButtonPress
# > * ButtonRelease
# > * Passive-grabbed KeyPress
# > * Passive-grabbed KeyRelease
#
# Since managing a window happens on a MapNotify (which is absent from this
# list), the window cannot accept input focus, so we should not try to focus
# the window at all.
subtest 'Window with WM_TAKE_FOCUS and without InputHint', sub {
    fresh_workspace;

    my $take_focus = $x->atom(name => 'WM_TAKE_FOCUS');

    my $window = open_window({
        dont_map => 1,
        protocols => [ $take_focus ],
    });

    # add an (empty) WM_HINTS property without the InputHint
    $window->delete_hint('input');

    $window->map;

    ok(!recv_take_focus($window), 'did not receive ClientMessage');

    done_testing;
};

# If the InputHint is unspecified, i3 should use the simpler method of focusing
# the window directly rather than using the WM_TAKE_FOCUS protocol.
# XXX: The code paths for an unspecified and set InputHint are
# nearly identical presently, so this is currently used also as a proxy test
# for the latter case.
subtest 'Window with WM_TAKE_FOCUS and unspecified InputHint', sub {
    fresh_workspace;

    my $take_focus = $x->atom(name => 'WM_TAKE_FOCUS');

    my $window = open_window({ protocols => [ $take_focus ] });

    ok(!recv_take_focus($window), 'did not receive ClientMessage');

    done_testing;
};

done_testing;
