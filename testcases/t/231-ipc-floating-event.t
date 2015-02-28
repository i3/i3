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
# Test that the window::floating event works correctly. This event should be
# emitted when a window transitions to or from the floating state.
# Bug still in: 4.8-7-gf4a8253
use i3test;

my $i3 = i3(get_socket_path());
$i3->connect->recv;

my $cv = AnyEvent->condvar;

$i3->subscribe({
        window => sub {
            my ($event) = @_;
            $cv->send($event) if $event->{change} eq 'floating';
        }
    })->recv;

my $t;
$t = AnyEvent->timer(
    after => 0.5,
    cb => sub {
        $cv->send(0);
    }
);

my $win = open_window();

cmd '[id="' . $win->{id} . '"] floating enable';
my $e = $cv->recv;

isnt($e, 0, 'floating a container should send an ipc window event');
is($e->{container}->{window}, $win->{id}, 'the event should contain information about the window');
is($e->{container}->{floating}, 'user_on', 'the container should be floating');

$cv = AnyEvent->condvar;
cmd '[id="' . $win->{id} . '"] floating disable';
my $e = $cv->recv;

isnt($e, 0, 'disabling floating on a container should send an ipc window event');
is($e->{container}->{window}, $win->{id}, 'the event should contain information about the window');
is($e->{container}->{floating}, 'user_off', 'the container should not be floating');

done_testing;
