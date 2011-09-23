#!perl
# vim:ts=4:sw=4:expandtab
# Checks if the focus is correctly restored, when creating a floating client
# over an unfocused tiling client and destroying the floating one again.

use i3test;

my $x = X11::XCB::Connection->new;

my $i3 = i3(get_socket_path());
fresh_workspace;

cmd 'split h';
my $tiled_left = open_standard_window($x);
my $tiled_right = open_standard_window($x);

sync_with_i3($x);

# Get input focus before creating the floating window
my $focus = $x->input_focus;

# Create a floating window which is smaller than the minimum enforced size of i3
my $window = open_standard_window($x, undef, 1);
sync_with_i3($x);

is($x->input_focus, $window->id, 'floating window focused');

$window->unmap;

# TODO: wait for unmap
sync_with_i3($x);

is($x->input_focus, $focus, 'Focus correctly restored');

done_testing;
