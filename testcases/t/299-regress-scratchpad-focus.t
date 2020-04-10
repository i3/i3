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
# Regression test: verify that a scratchpad container that was open in another
# workspace and is moved to the current workspace after a 'scratchpad show' is
# focused.
# Ticket: #3361
# Bug still in: 4.15-190-g4b3ff9cd
use i3test;

my $expected_focus = open_window;
cmd 'move to scratchpad';
cmd 'scratchpad show';
my $ws = fresh_workspace;
open_window;
cmd 'scratchpad show';
sync_with_i3;
is($x->input_focus, $expected_focus->id, 'scratchpad window brought from other workspace is focused');

done_testing;
