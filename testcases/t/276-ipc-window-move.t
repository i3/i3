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
# Tests that the ipc window::move event works properly
#
# Bug still in: 4.8-7-gf4a8253
use i3test;

my $dummy_window = open_window;
my $window = open_window;

sub move_subtest {
    my ($cmd) = @_;
    my $cv = AnyEvent->condvar;
    my @events = events_for(
	sub { cmd $cmd },
	'window');

    my @move = grep { $_->{change} eq 'move' } @events;
    is(scalar @move, 1, 'Received 1 window::move event');
    is($move[0]->{container}->{window}, $window->{id}, 'window id matches');
}

subtest 'move right', \&move_subtest, 'move right';
subtest 'move to workspace', \&move_subtest, 'move to workspace ws_new';

done_testing;
