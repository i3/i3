#!perl
# vim:ts=4:sw=4:expandtab
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
cmd 'level up';
sleep 0.25;

# make it floating
cmd 'mode toggle';
sleep 0.25;

# move the con outside the floating con
cmd 'move up';
sleep 0.25;

does_i3_live;

# move another con outside
cmd '[id="' . $mid->id . '"] focus';
cmd 'move up';
sleep 0.25;

does_i3_live;

done_testing;
