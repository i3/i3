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
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)

use i3test;

SKIP: {

    skip "AnyEvent::I3 too old (need >= 0.15)", 1 if $AnyEvent::I3::VERSION < 0.15;

my $i3 = i3(get_socket_path());
$i3->connect()->recv;

################################
# Window focus event
################################

cmd 'split h';

my $win0 = open_window;
my $win1 = open_window;
my $win2 = open_window;

my $focus = AnyEvent->condvar;

$i3->subscribe({
    window => sub {
        my ($event) = @_;
        $focus->send($event);
    }
})->recv;

my $t;
$t = AnyEvent->timer(
    after => 0.5,
    cb => sub {
        $focus->send(0);
    }
);

# ensure the rightmost window contains input focus
$i3->command('[id="' . $win2->id . '"] focus')->recv;
is($x->input_focus, $win2->id, "Window 2 focused");

cmd 'focus left';
my $event = $focus->recv;
is($event->{change}, 'focus', 'Focus event received');
is($focus->recv->{container}->{name}, 'Window 1', 'Window 1 focused');

$focus = AnyEvent->condvar;
cmd 'focus left';
$event = $focus->recv;
is($event->{change}, 'focus', 'Focus event received');
is($event->{container}->{name}, 'Window 0', 'Window 0 focused');

$focus = AnyEvent->condvar;
cmd 'focus right';
$event = $focus->recv;
is($event->{change}, 'focus', 'Focus event received');
is($event->{container}->{name}, 'Window 1', 'Window 1 focused');

$focus = AnyEvent->condvar;
cmd 'focus right';
$event = $focus->recv;
is($event->{change}, 'focus', 'Focus event received');
is($event->{container}->{name}, 'Window 2', 'Window 2 focused');

$focus = AnyEvent->condvar;
cmd 'focus right';
$event = $focus->recv;
is($event->{change}, 'focus', 'Focus event received');
is($event->{container}->{name}, 'Window 0', 'Window 0 focused');

$focus = AnyEvent->condvar;
cmd 'focus left';
$event = $focus->recv;
is($event->{change}, 'focus', 'Focus event received');
is($event->{container}->{name}, 'Window 2', 'Window 2 focused');

}

done_testing;
