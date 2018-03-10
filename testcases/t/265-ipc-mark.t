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
# Tests for the window::mark IPC event.
# Ticket: #2501
use i3test;

sub mark_subtest {
    my ($cmd) = @_;

    my @events = events_for(
	sub { cmd $cmd },
	'window');

    my @mark = grep { $_->{change} eq 'mark' } @events;
    is(scalar @mark, 1, 'Received 1 window::mark event');
}

###############################################################################
# Marking a container triggers a 'mark' event.
###############################################################################
fresh_workspace;
open_window;

subtest 'mark', \&mark_subtest, 'mark x';

###############################################################################
# Unmarking a container triggers a 'mark' event.
###############################################################################
fresh_workspace;
open_window;
cmd 'mark x';

subtest 'unmark', \&mark_subtest, 'unmark x';

###############################################################################

done_testing;
