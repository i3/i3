#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression test for moving a con outside of a floating con when there are no
# tiling cons on a workspace
#
use i3test;

sub sync_cmd {
    cmd @_;
    sync_with_i3;
}

my $tmp = fresh_workspace;

my $left = open_window;
my $mid = open_window;
my $right = open_window;

# go to workspace level
sync_cmd 'focus parent';

# make it floating
sync_cmd 'mode toggle';

# move the con outside the floating con
sync_cmd 'move up';

does_i3_live;

# move another con outside
sync_cmd '[id="' . $mid->id . '"] focus';
sync_cmd 'move up';

does_i3_live;

done_testing;
