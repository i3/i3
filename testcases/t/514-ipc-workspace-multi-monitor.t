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
# Ticket: #990
# Bug still in: 4.5.1-23-g82b5978

use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

my $i3 = i3(get_socket_path());

$i3->connect()->recv;

################################
# Workspaces requests and events
################################

my $old_ws = get_ws(focused_ws);

# Events

# We are switching to an empty workpspace on the output to the right from an empty workspace on the output on the left, so we expect
# to receive "init", "focus", and "empty".
my $focus = AnyEvent->condvar;
$i3->subscribe({
    workspace => sub {
        my ($event) = @_;
        if ($event->{change} eq 'focus') {
            $focus->send($event);
        }
    }
})->recv;

my $t;
$t = AnyEvent->timer(
    after => 0.5,
    cb => sub {
        $focus->send(0);
    }
);

cmd 'focus output right';

my $event = $focus->recv;

my $current_ws = get_ws(focused_ws);

ok($event, 'Workspace "focus" event received');
is($event->{current}->{id}, $current_ws->{id}, 'Event gave correct current workspace');
is($event->{old}->{id}, $old_ws->{id}, 'Event gave correct old workspace');

done_testing;
