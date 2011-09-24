#!perl
# vim:ts=4:sw=4:expandtab
# Checks if the focus is correctly restored, when creating a floating client
# over an unfocused tiling client and destroying the floating one again.

use i3test;

my $x = X11::XCB::Connection->new;

fresh_workspace;

cmd 'split h';
my $tiled_left = open_window($x);
my $tiled_right = open_window($x);

# Get input focus before creating the floating window
my $focus = $x->input_focus;

# Create a floating window which is smaller than the minimum enforced size of i3
my $window = open_floating_window($x);

is($x->input_focus, $window->id, 'floating window focused');

$window->unmap;

wait_for_unmap($x);

is($x->input_focus, $focus, 'Focus correctly restored');

done_testing;
