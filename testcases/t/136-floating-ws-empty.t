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
# Regression test: when only having a floating window on a workspace, it should
# not be deleted.

use i3test;

my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

#############################################################################
# 1: open a floating window, get it mapped
#############################################################################

ok(workspace_exists($tmp), "workspace $tmp exists");

# Create a floating window which is smaller than the minimum enforced size of i3
my $window = open_floating_window;
ok($window->mapped, 'Window is mapped');

# switch to a different workspace, see if the window is still mapped?

my $otmp = fresh_workspace;

ok(workspace_exists($otmp), "new workspace $otmp exists");
ok(workspace_exists($tmp), "old workspace $tmp still exists");

################################################################################
# 2: Similar test: Have two floating windows on a workspace, close one of them.
# The workspace should not be closed. Regression present until (including) commit
# 1f2c9306a27cced83ad960e929bb9e9a163b7843
################################################################################

$tmp = fresh_workspace;

ok(workspace_exists($tmp), "workspace $tmp exists");

# Create a floating window which is smaller than the minimum enforced size of i3
my $first = open_floating_window;
my $second = open_floating_window;
ok($first->mapped, 'Window is mapped');
ok($second->mapped, 'Window is mapped');

$otmp = fresh_workspace;

ok(workspace_exists($otmp), "new workspace $otmp exists");
ok(workspace_exists($tmp), "old workspace $tmp still exists");

$first->unmap;
wait_for_unmap $first;

ok(workspace_exists($otmp), "new workspace $otmp exists");
ok(workspace_exists($tmp), "old workspace $tmp still exists");

done_testing;
