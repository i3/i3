#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests if the WM_TAKE_FOCUS protocol is correctly handled by i3
#
use X11::XCB qw(:all);
use EV;
use AnyEvent;
use i3test;
use v5.10;

BEGIN {
    use_ok('X11::XCB::Window');
    use_ok('X11::XCB::Event::Generic');
    use_ok('X11::XCB::Event::MapNotify');
    use_ok('X11::XCB::Event::ClientMessage');
}

my $x = X11::XCB::Connection->new;
my $i3 = i3("/tmp/nestedcons");

subtest 'Window without WM_TAKE_FOCUS', sub {

    my $tmp = fresh_workspace;

    my $window = $x->root->create_child(
        class => WINDOW_CLASS_INPUT_OUTPUT,
        rect => [ 0, 0, 30, 30 ],
        background_color => '#00ff00',
        event_mask => [ 'structure_notify' ],
    );

    $window->name('Window 1');
    $window->map;

    my $cv = AE::cv;

    my $prep = EV::prepare sub {
        $x->flush;
    };

    my $check = EV::check sub {
        while (defined(my $event = $x->poll_for_event)) {
            if ($event->response_type == 161) {
                # clientmessage
                $cv->send(0);
            }
        }
    };

    my $w = EV::io $x->get_file_descriptor, EV::READ, sub {
        # do nothing, we only need this watcher so that EV picks up the events
    };

    # Trigger timeout after 1 second
    my $t = AE::timer 1, 0, sub {
        $cv->send(1);
    };

    my $result = $cv->recv;
    ok($result, 'cv result');
};

subtest 'Window with WM_TAKE_FOCUS', sub {

    my $tmp = fresh_workspace;

    my $window = $x->root->create_child(
        class => WINDOW_CLASS_INPUT_OUTPUT,
        rect => [ 0, 0, 30, 30 ],
        background_color => '#00ff00',
        event_mask => [ 'structure_notify' ],
        protocols => [ $x->atom(name => 'WM_TAKE_FOCUS') ],
    );

    $window->name('Window 1');
    $window->map;

    my $cv = AE::cv;

    my $prep = EV::prepare sub {
        $x->flush;
    };

    my $check = EV::check sub {
        while (defined(my $event = $x->poll_for_event)) {
            if ($event->response_type == 161) {
                $cv->send($event->data);
            }
        }
    };

    my $w = EV::io $x->get_file_descriptor, EV::READ, sub {
        # do nothing, we only need this watcher so that EV picks up the events
    };

    my $t = AE::timer 1, 0, sub {
        say "timer!";
        $cv->send(undef);
    };

    my $result = $cv->recv;
    ok(defined($result), 'got a ClientMessage');
    if (defined($result)) {
        my ($data, $time) = unpack("L2", $result);
        is($data, $x->atom(name => 'WM_TAKE_FOCUS')->id, 'first uint32_t contains WM_TAKE_FOCUS atom');
    }
};


done_testing;
