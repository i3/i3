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
# Regression test: verify layout toggle with invalid parameters does not set
# layout to L_DEFAULT, which crashes i3 upon the next IPC message.
# Ticket: #2903
# Bug still in: 4.14-87-g607e97e6
use i3test;

cmd 'layout toggle 1337 1337';

fresh_workspace;

does_i3_live;

done_testing;
