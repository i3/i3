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
# Test that the window::urgent event works correctly. The window::urgent event
# should be emitted when a window becomes urgent or loses its urgent status.
#
use i3test;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

force_display_urgency_hint 0ms
EOT

my $i3 = i3(get_socket_path());
$i3->connect()->recv;

my $cv;
$i3->subscribe({
    window => sub {
        my ($event) = @_;
        $cv->send($event) if $event->{change} eq 'urgent';
    }
})->recv;

my $t;
$t = AnyEvent->timer(
    after => 0.5,
    cb => sub {
        $cv->send(0);
    }
);

$cv = AnyEvent->condvar;
fresh_workspace;
my $win = open_window;
my $dummy_win = open_window;

$win->add_hint('urgency');
my $event = $cv->recv;

isnt($event, 0, 'an urgent con should emit the window::urgent event');
is($event->{container}->{window}, $win->{id}, 'the event should contain information about the window');
is($event->{container}->{urgent}, 1, 'the container should be urgent');

$cv = AnyEvent->condvar;
$win->delete_hint('urgency');
my $event = $cv->recv;

isnt($event, 0, 'an urgent con should emit the window::urgent event');
is($event->{container}->{window}, $win->{id}, 'the event should contain information about the window');
is($event->{container}->{urgent}, 0, 'the container should not be urgent');

done_testing;
