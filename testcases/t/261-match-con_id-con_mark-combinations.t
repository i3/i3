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
# Ticket: #2111
use i3test;

my ($ws);

###############################################################################
# Verify that con_id can be combined with other criteria
###############################################################################

$ws = fresh_workspace;
open_window(wm_class => 'matchme');

cmd '[con_id=__focused__ class=doesnotmatch] kill';
sync_with_i3;
is(@{get_ws($ws)->{nodes}}, 1, 'window was not killed');

cmd '[con_id=__focused__ class=matchme] kill';
sync_with_i3;
is(@{get_ws($ws)->{nodes}}, 0, 'window was killed');

###############################################################################
# Verify that con_mark can be combined with other criteria
###############################################################################

$ws = fresh_workspace;
open_window(wm_class => 'matchme');
cmd 'mark marked';

cmd '[con_mark=marked class=doesnotmatch] kill';
sync_with_i3;
is(@{get_ws($ws)->{nodes}}, 1, 'window was not killed');

cmd '[con_mark=marked class=matchme] kill';
sync_with_i3;
is(@{get_ws($ws)->{nodes}}, 0, 'window was killed');

###############################################################################

done_testing;
