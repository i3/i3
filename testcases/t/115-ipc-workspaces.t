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

################################
# Workspaces requests and events
################################

my $focused = get_ws(focused_ws());

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
            $init->send(1);
        } elsif ($event->{change} eq 'focus') {
            # Check that we have the old and new workspace
            $focus->send(
                $event->{current}->{name} == '2' &&
                $event->{old}->{name} == $focused->{name}
            );
        } elsif ($event->{change} eq 'empty') {
            $empty->send(1);
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

ok($init->recv, 'Workspace "init" event received');
ok($focus->recv, 'Workspace "focus" event received');
ok($empty->recv, 'Workspace "empty" event received');

done_testing;
