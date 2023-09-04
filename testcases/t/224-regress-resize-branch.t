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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Test that i3 does not crash when resizing a split container
# Ticket: #1220
# Bug still in: 4.7.2-128-g702906d
use i3test;

open_window;
open_window;

cmd 'split h';

open_window;

cmd 'focus parent, resize grow left';

does_i3_live;

done_testing;
