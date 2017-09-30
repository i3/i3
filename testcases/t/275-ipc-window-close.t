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
# Tests that the ipc close event works properly
#
# Bug still in: 4.8-7-gf4a8253
use i3test;

my $window = open_window;

my @events = events_for(
    sub {
	$window->unmap;
	sync_with_i3;
    },
    'window');

my @close = grep { $_->{change} eq 'close' } @events;
is(scalar @close, 1, 'Received 1 window::close event');
is($close[0]->{container}->{window}, $window->{id}, 'the event should contain information about the window');

done_testing;
