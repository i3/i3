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

use i3test;

################################
# Window focus event
################################

cmd 'split h';

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

subtest 'focus left (1)', \&focus_subtest, 'focus left', $win1->name;
subtest 'focus left (2)', \&focus_subtest, 'focus left', $win0->name;
subtest 'focus right (1)', \&focus_subtest, 'focus right', $win1->name;
subtest 'focus right (2)', \&focus_subtest, 'focus right', $win2->name;
subtest 'focus right (3)', \&focus_subtest, 'focus right', $win0->name;
subtest 'focus left', \&focus_subtest, 'focus left', $win2->name;

done_testing;
