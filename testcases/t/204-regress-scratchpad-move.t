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
# Moves the last window of a workspace to the scratchpad. The workspace will be
# cleaned up and previously, the subsequent focusing of a destroyed container
# would crash i3.
# Ticket: #913
# Bug still in: 4.4-97-gf767ac3
use i3test;

my $tmp = fresh_workspace;

# Open a new window which we can identify later on based on its WM_CLASS.
my $scratch = open_window(wm_class => 'special');

my $tmp2 = fresh_workspace;

cmd '[class="special"] move scratchpad';

does_i3_live;

done_testing;
