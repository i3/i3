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

my $left = open_window({ background_color => '#ff0000' });

is($x->input_focus, $left->id, 'left window focused');

diag("left = " . $left->id);

#####################################################################
# Open the right window
#####################################################################

my $right = open_window({ background_color => '#00ff00' });

diag("right = " . $right->id);

#####################################################################
# Set the right window to fullscreen
#####################################################################
cmd 'nop setting fullscreen';
cmd 'fullscreen';

#####################################################################
# Open a third window
#####################################################################

my $third = open_window({
        background_color => '#0000ff',
        name => 'Third window',
        dont_map => 1,
    });

$third->map;

sync_with_i3;

diag("third = " . $third->id);

# move the fullscreen window to a different ws

my $tmp2 = get_unused_workspace;

cmd "move workspace $tmp2";

# verify that the third window has the focus
is($x->input_focus, $third->id, 'third window focused');

################################################################################
# Ensure that moving a window to a workspace which has a fullscreen window does
# not focus it (otherwise the user cannot get out of fullscreen mode anymore).
################################################################################

$tmp = fresh_workspace;

my $fullscreen_window = open_window;
cmd 'fullscreen';

my $nodes = get_ws_content($tmp);
is(scalar @$nodes, 1, 'precisely one window');
is($nodes->[0]->{focused}, 1, 'fullscreen window focused');
my $old_id = $nodes->[0]->{id};

$tmp2 = fresh_workspace;
my $move_window = open_window;
cmd "move workspace $tmp";

cmd "workspace $tmp";

$nodes = get_ws_content($tmp);
is(scalar @$nodes, 2, 'precisely two windows');
is($nodes->[0]->{id}, $old_id, 'id unchanged');
is($nodes->[0]->{focused}, 1, 'fullscreen window focused');

done_testing;
