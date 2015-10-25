#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Test for the --no-auto-back-and-forth flag.
# Ticket: #2028
use i3test;

my ($first, $second, $third);
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

done_testing;
