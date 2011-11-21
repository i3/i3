#!perl
# vim:ts=4:sw=4:expandtab
#
# Test if new containers get focused when there is a fullscreen container at
# the time of launching the new one.
#
use i3test;

my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

#####################################################################
# open the left window
#####################################################################

my $left = open_window($x, { background_color => '#ff0000' });

is($x->input_focus, $left->id, 'left window focused');

diag("left = " . $left->id);

#####################################################################
# Open the right window
#####################################################################

my $right = open_window($x, { background_color => '#00ff00' });

diag("right = " . $right->id);

#####################################################################
# Set the right window to fullscreen
#####################################################################
cmd 'nop setting fullscreen';
cmd 'fullscreen';

#####################################################################
# Open a third window
#####################################################################

my $third = open_window($x, {
        background_color => '#0000ff',
        name => 'Third window',
        dont_map => 1,
    });

$third->map;

sync_with_i3 $x;

diag("third = " . $third->id);

# move the fullscreen window to a different ws

my $tmp2 = get_unused_workspace;

cmd "move workspace $tmp2";

# verify that the third window has the focus

sync_with_i3($x);

is($x->input_focus, $third->id, 'third window focused');

done_testing;
