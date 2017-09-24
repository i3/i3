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
# Checks the workspace "empty" event semantics.
#
use i3test;

################################################################################
# check that the workspace empty event is sent upon workspace switch when the
# old workspace is empty
################################################################################
subtest 'Workspace empty event upon switch', sub {
    my $ws2 = fresh_workspace;
    my $w2 = open_window();
    my $ws1 = fresh_workspace;
    my $w1 = open_window();

    cmd '[id="' . $w1->id . '"] kill';

    my $cond = AnyEvent->condvar;
    my @events = events_for(
	sub { cmd "workspace $ws2" },
	'workspace');

    is(scalar @events, 2, 'Received 2 event');
    is($events[1]->{change}, 'empty', '"Empty" event received upon workspace switch');
    is($events[1]->{current}->{name}, $ws1, '"current" property should be set to the workspace con');
};

################################################################################
# check that no workspace empty event is sent upon workspace switch if the
# workspace is not empty
################################################################################
subtest 'No workspace empty event', sub {
    my $ws2 = fresh_workspace;
    my $w2 = open_window();
    my $ws1 = fresh_workspace;
    my $w1 = open_window();

    my @events = events_for(
	sub { cmd "workspace $ws2" },
	'workspace');

    is(scalar @events, 1, 'Received 1 event');
    is($events[0]->{change}, 'focus', 'Event change is "focus"');
};

################################################################################
# check that workspace empty event is sent when the last window has been closed
# on invisible workspace
################################################################################
subtest 'Workspace empty event upon window close', sub {
    my $ws1 = fresh_workspace;
    my $w1 = open_window();
    my $ws2 = fresh_workspace;
    my $w2 = open_window();

    my @events = events_for(
	sub {
	    $w1->unmap;
	    sync_with_i3;
	},
	'workspace');

    is(scalar @events, 1, 'Received 1 event');
    is($events[0]->{change}, 'empty', '"Empty" event received upon window close');
    is($events[0]->{current}->{name}, $ws1, '"current" property should be set to the workspace con');
};

done_testing;
