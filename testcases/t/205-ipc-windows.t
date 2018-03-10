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

my $new = AnyEvent->condvar;
my $focus = AnyEvent->condvar;

my @events = events_for(
    sub { open_window },
    'window');

is(scalar @events, 2, 'Received 2 events');
is($events[0]->{container}->{focused}, 0, 'Window "new" event received');
is($events[1]->{container}->{focused}, 1, 'Window "focus" event received');

done_testing;
