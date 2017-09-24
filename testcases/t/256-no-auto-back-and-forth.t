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
# Test for the --no-auto-back-and-forth flag.
# Ticket: #2028
use i3test;

my ($first, $second, $third, $con);
$first = "1:first";
$second = "2:second";
$third = "3:third";

###############################################################################
# Switching to another workspace when passing --no-auto-back-and-forth works
# as if the flag wasn't set.
###############################################################################

cmd qq|workspace "$first"|;
ok(get_ws($first)->{focused}, 'first workspace is focused');

cmd qq|workspace --no-auto-back-and-forth "$second"|;
ok(get_ws($second)->{focused}, 'second workspace is focused');

cmd qq|workspace --no-auto-back-and-forth number "$third"|;
ok(get_ws($third)->{focused}, 'third workspace is focused');

###############################################################################
# Switching to the focused workspace when passing --no-auto-back-and-forth
# is a no-op.
###############################################################################

cmd qq|workspace "$second"|;
cmd qq|workspace "$first"|;
ok(get_ws($first)->{focused}, 'first workspace is focused');

cmd qq|workspace --no-auto-back-and-forth "$first"|;
ok(get_ws($first)->{focused}, 'first workspace is still focused');

cmd qq|workspace --no-auto-back-and-forth number "$first"|;
ok(get_ws($first)->{focused}, 'first  workspace is still focused');

###############################################################################
# Moving a window to another workspace when passing --no-auto-back-and-forth
# works as if the flag wasn't set.
###############################################################################

cmd qq|workspace "$third"|;
cmd qq|workspace "$second"|;
cmd qq|workspace "$first"|;
$con = open_window;
cmd 'mark mywindow';

cmd qq|move --no-auto-back-and-forth window to workspace "$second"|;
is(@{get_ws($second)->{nodes}}, 1, 'window was moved to second workspace');
cmd qq|[con_mark=mywindow] move window to workspace "$first"|;

cmd qq|move --no-auto-back-and-forth window to workspace number "$third"|;
is(@{get_ws($third)->{nodes}}, 1, 'window was moved to third workspace');
cmd qq|[con_mark=mywindow] move window to workspace "$first"|;

cmd '[con_mark=mywindow] kill';

###############################################################################
# Moving a window to the same workspace when passing --no-auto-back-and-forth
# is a no-op.
###############################################################################

cmd qq|workspace "$second"|;
cmd qq|workspace "$first"|;
$con = open_window;
cmd 'mark mywindow';

cmd qq|move --no-auto-back-and-forth window to workspace "$first"|;
is(@{get_ws($first)->{nodes}}, 1, 'window is still on first workspace');
cmd qq|[con_mark=mywindow] move window to workspace "$first"|;

cmd qq|move --no-auto-back-and-forth window to workspace number "$first"|;
is(@{get_ws($first)->{nodes}}, 1, 'window is still on first workspace');
cmd qq|[con_mark=mywindow] move window to workspace "$first"|;

cmd '[con_mark=mywindow] kill';

###############################################################################

done_testing;
