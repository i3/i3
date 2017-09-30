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

my $window = open_window(name => 'Window 0');

my @events = events_for(
    sub {
	$window->name('New Window Title');
	sync_with_i3;
    },
    'window');

is(scalar @events, 1, 'Received 1 event');
is($events[0]->{change}, 'title', 'Window title change event received');
is($events[0]->{container}->{name}, 'New Window Title', 'Window title changed');

done_testing;
