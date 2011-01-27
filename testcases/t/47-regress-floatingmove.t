#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression test for moving a con outside of a floating con when there are no
# tiling cons on a workspace
#
use X11::XCB qw(:all);
use Time::HiRes qw(sleep);
use i3test tests => 2;

BEGIN {
    use_ok('X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;

my $tmp = get_unused_workspace;
cmd "workspace $tmp";

my $left = open_standard_window($x);
sleep 0.25;
my $mid = open_standard_window($x);
sleep 0.25;

# go to workspace level
cmd 'level up';
sleep 0.25;

# make it floating
cmd 'mode toggle';
sleep 0.25;

# move the con outside the floating con
cmd 'move before v';
sleep 0.25;

my $tree = i3('/tmp/nestedcons')->get_tree->recv;
my @nodes = @{$tree->{nodes}};
ok(@nodes > 0, 'i3 still lives');
