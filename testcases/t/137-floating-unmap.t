#!perl
# vim:ts=4:sw=4:expandtab
# Regression test: Floating windows were not correctly unmapped when switching
# to a different workspace.

use i3test;

my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

#############################################################################
# 1: open a floating window, get it mapped
#############################################################################

# Create a floating window which is smaller than the minimum enforced size of i3
my $window = open_floating_window;
ok($window->mapped, 'Window is mapped');

# switch to a different workspace, see if the window is still mapped?

my $otmp = fresh_workspace;

sync_with_i3($x);

ok(!$window->mapped, 'Window is not mapped after switching ws');

cmd "nop testcase done";

done_testing;
