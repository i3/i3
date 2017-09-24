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
# When using a command which moves a window to scratchpad from an invisible
# (e.g. unfocused) workspace and immediately shows that window again, i3
# crashed.
# Bug still in: 4.2-305-g22922a9
use i3test;

my $ws1 = fresh_workspace;
my $invisible_window = open_window;
my $other_focusable_window = open_window;

my $ws2 = fresh_workspace;
my $id = $invisible_window->id;
cmd qq|[id="$id"] move scratchpad, scratchpad show|;

does_i3_live;

done_testing;
