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
# Ticket: #2091
use i3test;

my $ws = fresh_workspace;
open_window;

my $result = cmd '[con_id=foobar] kill';
is($result->[0]->{success}, 0, 'command was unsuccessful');
is($result->[0]->{error}, 'Invalid match: invalid con_id', 'correct error is returned');

done_testing;
