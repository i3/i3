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
# Make sure the command `move <direction>` properly sends the workspace focus
# ipc event required for i3bar to be properly updated and redrawn.
#
# Bug still in: 4.6-195-g34232b8
use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
workspace ws-left output fake-0
workspace ws-right output fake-1
EOT

my $pid = launch_with_config($config);

my $i3 = i3(get_socket_path());
$i3->connect()->recv;

# subscribe to the 'focus' ipc event
my $focus = AnyEvent->condvar;
$i3->subscribe({
    workspace => sub {
        my ($event) = @_;
        if ($event->{change} eq 'focus') {
            $focus->send($event);
        }
    }
})->recv;

# give up after 0.5 seconds
my $timer = AnyEvent->timer(
    after => 0.5,
    cb => sub {
        $focus->send(0);
    }
);

# open two windows on the left output
cmd 'workspace ws-left';
open_window;
open_window;

# move a window over to the right output
cmd 'move right';
my $event = $focus->recv;

ok($event, 'moving from workspace with two windows triggered focus ipc event');
is($event->{current}->{name}, 'ws-right', 'focus event gave the right workspace');
is(@{$event->{current}->{nodes}}, 1, 'focus event gave the right number of windows on the workspace');

# reset and try again
$focus = AnyEvent->condvar;
cmd 'workspace ws-left';
$focus->recv;

$focus = AnyEvent->condvar;
cmd 'move right';
$event = $focus->recv;
ok($event, 'moving from workspace with one window triggered focus ipc event');
is($event->{current}->{name}, 'ws-right', 'focus event gave the right workspace');
is(@{$event->{current}->{nodes}}, 2, 'focus event gave the right number of windows on the workspace');

exit_gracefully($pid);

done_testing;
