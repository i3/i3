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
# Regression test for moving a con outside of a floating con when there are no
# tiling cons on a workspace
#
use i3test;

my $tmp = fresh_workspace;

my $left = open_window;
my $mid = open_window;
my $right = open_window;

# go to workspace level
cmd 'focus parent';

# make it floating
cmd 'mode toggle';

# move the con outside the floating con
cmd 'move up';

does_i3_live;

# move another con outside
cmd '[id="' . $mid->id . '"] focus';
cmd 'move up';

does_i3_live;

done_testing;
