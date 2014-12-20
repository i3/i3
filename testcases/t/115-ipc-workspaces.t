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

use i3test;

my $i3 = i3(get_socket_path());
$i3->connect()->recv;

################################
# Workspaces requests and events
################################

my $old_ws = get_ws(focused_ws());

# Events

# We are switching to an empty workpspace from an empty workspace, so we expect
# to receive "init", "focus", and "empty".
my $init = AnyEvent->condvar;
my $focus = AnyEvent->condvar;
my $empty = AnyEvent->condvar;
$i3->subscribe({
    workspace => sub {
        my ($event) = @_;
        if ($event->{change} eq 'init') {
            $init->send($event);
        } elsif ($event->{change} eq 'focus') {
            $focus->send($event);
        } elsif ($event->{change} eq 'empty') {
            $empty->send($event);
        }
    }
})->recv;

cmd 'workspace 2';

my $t;
$t = AnyEvent->timer(
    after => 0.5,
    cb => sub {
        $init->send(0);
        $focus->send(0);
        $empty->send(0);
    }
);

my $init_event = $init->recv;
my $focus_event = $focus->recv;
my $empty_event = $empty->recv;

my $current_ws = get_ws(focused_ws());

ok($init_event, 'workspace "init" event received');
is($init_event->{current}->{id}, $current_ws->{id}, 'the "current" property should contain the initted workspace con');

ok($focus_event, 'workspace "focus" event received');
is($focus_event->{current}->{id}, $current_ws->{id}, 'the "current" property should contain the focused workspace con');
is($focus_event->{old}->{id}, $old_ws->{id}, 'the "old" property should contain the workspace con that was focused last');

ok($empty_event, 'workspace "empty" event received');
is($empty_event->{current}->{id}, $old_ws->{id}, 'the "current" property should contain the emptied workspace con');

done_testing;
