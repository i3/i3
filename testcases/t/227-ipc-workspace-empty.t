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
# Checks the workspace "empty" event semantics.
#
use i3test;

SKIP: {

    skip "AnyEvent::I3 too old (need >= 0.15)", 1 if $AnyEvent::I3::VERSION < 0.15;

################################################################################
# check that the workspace empty event is send upon workspace switch when the
# old workspace is empty
################################################################################
subtest 'Workspace empty event upon switch', sub {
    my $ws2 = fresh_workspace;
    my $w2 = open_window();
    my $ws1 = fresh_workspace;
    my $w1 = open_window();

    cmd '[id="' . $w1->id . '"] kill';

    my $cond = AnyEvent->condvar;
    my $client = i3(get_socket_path(0));
    $client->connect()->recv;
    $client->subscribe({
        workspace => sub {
            my ($event) = @_;
            $cond->send($event);
        }
    })->recv;

    cmd "workspace $ws2";

    sync_with_i3;

    my $event = $cond->recv;
    is($event->{change}, 'empty', '"Empty" event received upon workspace switch');
    is($event->{current}->{name}, $ws1, '"current" property should be set to the workspace con');
};

################################################################################
# check that no workspace empty event is send upon workspace switch if the
# workspace is not empty
################################################################################
subtest 'No workspace empty event', sub {
    my $ws2 = fresh_workspace;
    my $w2 = open_window();
    my $ws1 = fresh_workspace;
    my $w1 = open_window();

    my @events;
    my $cond = AnyEvent->condvar;
    my $client = i3(get_socket_path(0));
    $client->connect()->recv;
    $client->subscribe({
        workspace => sub {
            my ($event) = @_;
            push @events, $event;
        }
    })->recv;

    # Wait for the workspace event on a new connection. Events will be delivered
    # to older connections earlier, so by the time it arrives here, it should be
    # in @events already.
    my $ws_event_block_conn = i3(get_socket_path(0));
    $ws_event_block_conn->connect()->recv;
    $ws_event_block_conn->subscribe({ workspace => sub { $cond->send(1) }});

    cmd "workspace $ws2";

    sync_with_i3;

    my @expected_events = grep { $_->{change} eq 'focus' } @events;
    my @empty_events = grep { $_->{change} eq 'empty' } @events;
    is(@expected_events, 1, '"Focus" event received');
    is(@empty_events, 0, 'No "empty" events received');
};

################################################################################
# check that workspace empty event is send when the last window has been closed
# on invisible workspace
################################################################################
subtest 'Workspace empty event upon window close', sub {
    my $ws1 = fresh_workspace;
    my $w1 = open_window();
    my $ws2 = fresh_workspace;
    my $w2 = open_window();

    my $cond = AnyEvent->condvar;
    my $client = i3(get_socket_path(0));
    $client->connect()->recv;
    $client->subscribe({
        workspace => sub {
            my ($event) = @_;
            $cond->send($event);
        }
    })->recv;

    cmd '[id="' . $w1->id . '"] kill';

    sync_with_i3;

    my $event = $cond->recv;
    is($event->{change}, 'empty', '"Empty" event received upon window close');
    is($event->{current}->{name}, $ws1, '"current" property should be set to the workspace con');
};

}

done_testing;
