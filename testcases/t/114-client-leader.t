#!perl
# vim:ts=4:sw=4:expandtab

use i3test;

my $tmp = fresh_workspace;

####################################################################################
# first part: test if a floating window will be correctly positioned above its leader
#
# This is verified by opening two windows, then opening a floating window above the
# right one, then above the left one. If the floating windows are all positioned alike,
# one of both (depending on your screen resolution) will be positioned wrong.
####################################################################################

my $left = open_window($x, { name => 'Left' });
my $right = open_window($x, { name => 'Right' });

my ($abs, $rgeom) = $right->rect;

my $child = open_floating_window($x, {
        dont_map => 1,
        name => 'Child window',
    });
$child->client_leader($right);
$child->map;

ok(wait_for_map($x), 'child window mapped');

my $cgeom;
($abs, $cgeom) = $child->rect;
cmp_ok($cgeom->x, '>=', $rgeom->x, 'Child X >= right container X');

my $child2 = open_floating_window($x, {
        dont_map => 1,
        name => 'Child window 2',
    });
$child2->client_leader($left);
$child2->map;

ok(wait_for_map($x), 'second child window mapped');

($abs, $cgeom) = $child2->rect;
cmp_ok(($cgeom->x + $cgeom->width), '<', $rgeom->x, 'child above left window');

# check wm_transient_for
my $fwindow = open_window($x, { dont_map => 1 });
$fwindow->transient_for($right);
$fwindow->map;

ok(wait_for_map($x), 'transient window mapped');

my ($absolute, $top) = $fwindow->rect;
ok($absolute->{x} != 0 && $absolute->{y} != 0, 'i3 did not map it to (0x0)');

SKIP: {
    skip "(workspace placement by client_leader not yet implemented)", 3;

#####################################################################
# Create a parent window
#####################################################################

my $window = open_window($x, { dont_map => 1, name => 'Parent window' });
$window->map;

ok(wait_for_map($x), 'parent window mapped');

#########################################################################
# Switch to a different workspace and open a child window. It should be opened
# on the old workspace.
#########################################################################
fresh_workspace;

my $child = open_window($x, { dont_map => 1, name => 'Child window' });
$child->client_leader($window);
$child->map;

ok(wait_for_map($x), 'child window mapped');

isnt($x->input_focus, $child->id, "Child window focused");

# Switch back
cmd "workspace $tmp";

is($x->input_focus, $child->id, "Child window focused");

}

done_testing;
