#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression test for moving a con outside of a floating con when there are no
# tiling cons on a workspace
#
use X11::XCB qw(:all);
use Time::HiRes qw(sleep);
use i3test;

BEGIN {
    use_ok('X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;

my $tmp = fresh_workspace;

my $left = open_standard_window($x);
my $mid = open_standard_window($x);
my $right = open_standard_window($x);

# go to workspace level
cmd 'level up';
sleep 0.25;

# make it floating
cmd 'mode toggle';
sleep 0.25;

# move the con outside the floating con
cmd 'move before v';
sleep 0.25;

does_i3_live;

# move another con outside
cmd '[id="' . $mid->id . '"] focus';
cmd 'move before v';
sleep 0.25;

does_i3_live;

done_testing;
