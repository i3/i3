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
use i3test;

subtest 'Window without WM_TAKE_FOCUS', sub {
    fresh_workspace;

    my $window = open_window;
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

    ok($first_event_is_clientmessage, 'did not receive ClientMessage');

    done_testing;
};

subtest 'Window with WM_TAKE_FOCUS', sub {
    fresh_workspace;

    my $take_focus = $x->atom(name => 'WM_TAKE_FOCUS');

    my $window = open_window({
        dont_map => 1,
        protocols => [ $take_focus ],
    });

    $window->map;

    ok(wait_for_event(1, sub {
        return 0 unless $_[0]->{response_type} == 161;
        my ($data, $time) = unpack("L2", $_[0]->{data});
        return ($data == $take_focus->id);
    }), 'got ClientMessage with WM_TAKE_FOCUS atom');

    done_testing;
};


done_testing;
