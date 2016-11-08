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
# Tests for the window::mark IPC event.
# Ticket: #2501
use i3test;

my ($i3, $timer, $event, $mark);

$i3 = i3(get_socket_path());
$i3->connect()->recv;

$i3->subscribe({
    window => sub {
        my ($event) = @_;
        return unless defined $mark;
        return unless $event->{change} eq 'mark';

        $mark->send($event);
    }
})->recv;

$timer = AnyEvent->timer(
    after => 0.5,
    cb => sub {
        $mark->send(0);
    }
);

###############################################################################
# Marking a container triggers a 'mark' event.
###############################################################################
fresh_workspace;
open_window;

$mark = AnyEvent->condvar;
cmd 'mark x';

$event = $mark->recv;
ok($event, 'window::mark event has been received');

###############################################################################
# Unmarking a container triggers a 'mark' event.
###############################################################################
fresh_workspace;
open_window;
cmd 'mark x';

$mark = AnyEvent->condvar;
cmd 'unmark x';

$event = $mark->recv;
ok($event, 'window::mark event has been received');

###############################################################################

done_testing;
