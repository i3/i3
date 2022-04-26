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

use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

# fake-1 under fake-0 to not interfere with left/right wraping
fake-outputs 1024x768+0+0,1024x768+0+1024
workspace X output fake-1
EOT

################################
# Window focus event
################################

my $ws = fresh_workspace(output => 0);
my $win0 = open_window;
my $win1 = open_window;
my $win2 = open_window;

# ensure the rightmost window contains input focus
cmd '[id="' . $win2->id . '"] focus';
is($x->input_focus, $win2->id, "Window 2 focused");

sub focus_subtest {
    my ($cmd, $name) = @_;

    my $focus = AnyEvent->condvar;

    my @events = events_for(
	sub { cmd $cmd },
	'window');

    is(scalar @events, 1, 'Received 1 event');
    is($events[0]->{change}, 'focus', 'Focus event received');
    is($events[0]->{container}->{name}, $name, "$name focused");
}

sub kill_subtest {
    my ($cmd, $name) = @_;

    my $focus = AnyEvent->condvar;

    my @events = events_for(
	sub {
	    cmd $cmd;
	    # Sync to make sure x_window_kill() calls have taken effect.
	    sync_with_i3;
	},
	'window');

    is(scalar @events, 1, 'Received 1 event');
    is($events[0]->{change}, 'close', 'Close event received');
    is($events[0]->{container}->{name}, $name, "$name closed");
}

subtest 'focus left (1)', \&focus_subtest, 'focus left', $win1->name;
subtest 'focus left (2)', \&focus_subtest, 'focus left', $win0->name;
subtest 'focus right (1)', \&focus_subtest, 'focus right', $win1->name;
subtest 'focus right (2)', \&focus_subtest, 'focus right', $win2->name;
subtest 'focus right (3)', \&focus_subtest, 'focus right', $win0->name;
subtest 'focus left', \&focus_subtest, 'focus left', $win2->name;
subtest 'kill doesn\'t produce focus event', \&kill_subtest, '[id=' . $win1->id . '] kill', $win1->name;

# See issue #3562. We need to switch to an existing workspace on the second
# output to trigger the bug.
cmd 'workspace X';
subtest 'workspace focus', \&focus_subtest, "workspace $ws", $win2->name;

sub scratchpad_subtest {
    my ($cmd, $name) = @_;

    my $focus = AnyEvent->condvar;

    my @events = events_for(
	sub { cmd $cmd },
	'window');

    is(scalar @events, 2, 'Received 2 events');
    is($events[0]->{change}, 'move', 'Move event received');
    is($events[0]->{container}->{nodes}->[0]->{name}, $name, "$name moved");
    is($events[1]->{change}, 'focus', 'Focus event received');
    is($events[1]->{container}->{name}, $name, "$name focused");
}

fresh_workspace;
my $win = open_window;
cmd 'move scratchpad';
subtest 'scratchpad', \&scratchpad_subtest, '[id=' . $win->id . '] scratchpad show', $win->name;

done_testing;
