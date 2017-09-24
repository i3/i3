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
# Regression: resizing a floating split container leads to a crash.
# (Ticket #588, present until 4412ccbe5a4fad8a4cd594e6f10f937515a4d37c)
#
use i3test;

my $tmp = fresh_workspace;

my $first = open_window;
cmd 'split v';
my $second = open_window;

cmd 'focus parent';
cmd 'floating toggle';
cmd 'layout stacking';

cmd 'resize grow up 10 px or 10 ppt';

does_i3_live;

done_testing;
