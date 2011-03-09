#!perl
# vim:ts=4:sw=4:expandtab
# Regression test: Floating windows were not correctly unmapped when switching
# to a different workspace.

use i3test;
use X11::XCB qw(:all);

BEGIN {
    use_ok('X11::XCB::Window');
}

my $i3 = i3("/tmp/nestedcons");

my $tmp = fresh_workspace;

#############################################################################
# 1: open a floating window, get it mapped
#############################################################################

my $x = X11::XCB::Connection->new;

# Create a floating window which is smaller than the minimum enforced size of i3
my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#C0C0C0',
    # replace the type with 'utility' as soon as the coercion works again in X11::XCB
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
);

isa_ok($window, 'X11::XCB::Window');

$window->map;

sleep 0.25;

ok($window->mapped, 'Window is mapped');

# switch to a different workspace, see if the window is still mapped?

my $otmp = fresh_workspace;

sleep 0.25;

ok(!$window->mapped, 'Window is not mapped after switching ws');

cmd "nop testcase done";

done_testing;
