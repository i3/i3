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
# Regression: Checks if i3 still lives after using 'focus mode_toggle' on an
# empty workspace. This regression was fixed in
# 0848844f2d41055f6ffc69af1149d7a873460976.
#
use i3test;
use v5.10;

my $tmp = fresh_workspace;

cmd 'focus mode_toggle';

does_i3_live;

done_testing;
