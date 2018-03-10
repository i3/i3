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
# Tests that the ipc window::fullscreen_mode event works properly
#
# Bug still in: 4.7.2-135-g7deb23c
use i3test;

open_window;

sub fullscreen_subtest {
    my ($want) = @_;
    my @events = events_for(
	sub { cmd 'fullscreen' },
	'window');

    is(scalar @events, 1, 'Received 1 event');
    is($events[0]->{container}->{fullscreen_mode}, $want, "fullscreen_mode now $want");
}

subtest 'fullscreen on', \&fullscreen_subtest, 1;
subtest 'fullscreen off', \&fullscreen_subtest, 0;

done_testing;
