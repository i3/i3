#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests if the WM_TAKE_FOCUS protocol is correctly handled by i3
#
use i3test;

subtest 'Window without WM_TAKE_FOCUS', sub {
    fresh_workspace;

    my $window = open_window;

    ok(!wait_for_event(1, sub { $_[0]->{response_type} == 161 }), 'did not receive ClientMessage');

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
