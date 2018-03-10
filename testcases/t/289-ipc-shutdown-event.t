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
#
# Test the ipc shutdown event. This event is triggered when the connection to
# the ipc is about to shutdown because of a user action such as with a
# `restart` or `exit` command. The `change` field indicates why the ipc is
# shutting down. It can be either "restart" or "exit".
#
# Ticket: #2318
# Bug still in: 4.12-46-g2123888
use i3test;

# We cannot use events_for in this test as we cannot send events after
# issuing the restart/shutdown command.

my $i3 = i3(get_socket_path());
$i3->connect->recv;

my $cv = AnyEvent->condvar;
my $timer = AnyEvent->timer(after => 0.5, interval => 0, cb => sub { $cv->send(0); });

$i3->subscribe({
        shutdown => sub {
            $cv->send(shift);
        }
    })->recv;

cmd 'restart';

my $e = $cv->recv;

diag "Event:\n", Dumper($e);
ok($e, 'the shutdown event should emit when the ipc is restarted by command');
is($e->{change}, 'restart', 'the `change` field should tell the reason for the shutdown');

# restarting kills the ipc client so we have to make a new one
$i3 = i3(get_socket_path());
$i3->connect->recv;

$cv = AnyEvent->condvar;
$timer = AnyEvent->timer(after => 0.5, interval => 0, cb => sub { $cv->send(0); });

$i3->subscribe({
        shutdown => sub {
            $cv->send(shift);
        }
    })->recv;

cmd 'exit';

$e = $cv->recv;

diag "Event:\n", Dumper($e);
ok($e, 'the shutdown event should emit when the ipc is exited by command');
is($e->{change}, 'exit', 'the `change` field should tell the reason for the shutdown');

done_testing;
