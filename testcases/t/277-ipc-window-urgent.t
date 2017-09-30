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
# Test that the window::urgent event works correctly. The window::urgent event
# should be emitted when a window becomes urgent or loses its urgent status.
#
use i3test;

fresh_workspace;
my $win = open_window;
my $dummy_win = open_window;

sub urgency_subtest {
    my ($subscribecb, $win, $want) = @_;

    my @events = events_for(
	$subscribecb,
	'window');

    my @urgent = grep { $_->{change} eq 'urgent' } @events;
    is(scalar @urgent, 1, 'Received 1 window::urgent event');
    is($urgent[0]->{container}->{window}, $win->{id}, "window id matches");
    is($urgent[0]->{container}->{urgent}, $want, "urgent is $want");
}

subtest "urgency set", \&urgency_subtest,
    sub {
	$win->add_hint('urgency');
	sync_with_i3;
    },
    $win,
    1;

subtest "urgency unset", \&urgency_subtest,
    sub {
	$win->delete_hint('urgency');
	sync_with_i3;
    },
    $win,
    0;

done_testing;
