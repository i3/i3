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
# Ticket: #990
# Bug still in: 4.5.1-23-g82b5978

use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

my $pid = launch_with_config($config);

my $i3 = i3(get_socket_path());

$i3->connect()->recv;

################################
# Workspaces requests and events
################################

my $focused = get_ws(focused_ws());

# Events

# We are switching to an empty workpspace on the output to the right from an empty workspace on the output on the left, so we expect
# to receive "init", "focus", and "empty".
my $focus = AnyEvent->condvar;
$i3->subscribe({
    workspace => sub {
        my ($event) = @_;
        if ($event->{change} eq 'focus') {
            # Check that we have the old and new workspace
            $focus->send(
                $event->{current}->{name} == '2' &&
                $event->{old}->{name} == $focused->{name}
            );
        }
    }
})->recv;

cmd 'focus output right';

my $t;
$t = AnyEvent->timer(
    after => 0.5,
    cb => sub {
        $focus->send(0);
    }
);

ok($focus->recv, 'Workspace "focus" event received');

exit_gracefully($pid);

done_testing;
