#!perl
# vim:ts=4:sw=4:expandtab
# Regression test: when only having a floating window on a workspace, it should not be deleted.

use i3test;
use X11::XCB qw(:all);

BEGIN {
    use_ok('X11::XCB::Window');
}

my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

#############################################################################
# 1: open a floating window, get it mapped
#############################################################################

ok(workspace_exists($tmp), "workspace $tmp exists");

my $x = X11::XCB::Connection->new;

# Create a floating window which is smaller than the minimum enforced size of i3
my $window = open_floating_window($x);
ok($window->mapped, 'Window is mapped');

# switch to a different workspace, see if the window is still mapped?

my $otmp = fresh_workspace;

ok(workspace_exists($otmp), "new workspace $otmp exists");
ok(workspace_exists($tmp), "old workspace $tmp still exists");

done_testing;
