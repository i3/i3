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
# Test that the window::floating event works correctly. This event should be
# emitted when a window transitions to or from the floating state.
# Bug still in: 4.8-7-gf4a8253
use i3test;

sub floating_subtest {
    my ($win, $cmd, $want) = @_;

    my @events = events_for(
	sub { cmd $cmd },
	'window');

    my @floating = grep { $_->{change} eq 'floating' } @events;
    is(scalar @floating, 1, 'Received 1 floating event');
    is($floating[0]->{container}->{window}, $win->{id}, "window id matches");
    is($floating[0]->{container}->{floating}, $want, "floating is $want");
}

my $win = open_window();

subtest 'floating enable', \&floating_subtest, $win, '[id="' . $win->{id} . '"] floating enable', 'user_on';
subtest 'floating disable', \&floating_subtest, $win, '[id="' . $win->{id} . '"] floating disable', 'user_off';

done_testing;
