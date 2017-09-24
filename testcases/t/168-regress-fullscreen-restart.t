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
# Verifies that i3 survives inplace restarts with fullscreen containers
#
use i3test;

fresh_workspace;

open_window;
open_window;

cmd 'layout stacking';
sync_with_i3;

cmd 'fullscreen';
sync_with_i3;

cmd 'restart';

does_i3_live;

done_testing;
