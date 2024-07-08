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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Test if new containers get focused when there is a fullscreen container at
# the time of launching the new one. Also make sure that focusing containers
# in other workspaces work even when there is a fullscreen container.
#
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT
# Screen setup looks like this:
# +----+----+
# | S1 | S2 |
# +----+----+

sub verify_focus {
    # Report original line
    local $Test::Builder::Level = $Test::Builder::Level + 1;

    my ($expected, $msg) = @_;
    $expected = $expected->id;

    # Ensure the expected focus if the test fails.
    cmd "[id=$expected] focus" unless is($x->input_focus, $expected, $msg);
}

################################################################################
# Verify that window opened behind fullscreen window will get focus after the
# fullscreen window gets moved to a different workspace.
################################################################################

fresh_workspace;
my $left = open_window;
verify_focus($left, 'left window focused');
diag("left = " . $left->id);

my $right = open_window;
cmd 'fullscreen';
diag("right = " . $right->id);

# Open a third window. Since we're fullscreen, the window won't be mapped, so
# don't wait for it to be mapped. Instead, just send the map request and sync
# with i3 to make sure i3 recognizes it.
my $third = open_window({dont_map => 1});
$third->map;
sync_with_i3;
diag("third = " . $third->id);

# Move the window to a different workspace, and verify that the third window now
# gets focused in the current workspace.
my $tmp2 = get_unused_workspace;
cmd "move workspace $tmp2";
verify_focus($third, 'third window focused');

kill_all_windows;

################################################################################
# Ensure that moving a window to a workspace which has a fullscreen window does
# not focus it (otherwise the user cannot get out of fullscreen mode anymore).
################################################################################

my $tmp = fresh_workspace;

open_window;
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

kill_all_windows;

################################################################################
# Ensure it's possible to change focus if it doesn't escape the fullscreen
# container with fullscreen global. We can't even focus a container in a
# different workspace.
################################################################################

$tmp = fresh_workspace(output => 1);
my $diff_ws = open_window;

$tmp2 = fresh_workspace(output => 0);
$left = open_window;
my $right1 = open_window;
cmd 'split v';
my $right2 = open_window;

cmd 'focus parent';
cmd 'fullscreen global';

cmd '[id="' . $right1->id . '"] focus';
verify_focus($right1, 'upper right window focused');

cmd '[id="' . $right2->id . '"] focus';
verify_focus($right2, 'bottom right window focused');

cmd 'focus parent';
isnt($x->input_focus, $right2->id, 'bottom right window no longer focused');

cmd 'focus child';
verify_focus($right2, 'bottom right window focused again');

cmd 'focus up';
verify_focus($right1, 'allowed focus up');

cmd 'focus down';
verify_focus($right2, 'allowed focus down');

cmd 'focus left';
verify_focus($right2, 'prevented focus left');

cmd 'focus right';
verify_focus($right2, 'prevented focus right');

cmd 'focus down';
verify_focus($right1, 'allowed focus wrap (down)');

cmd 'focus up';
verify_focus($right2, 'allowed focus wrap (up)');

################################################################################
# (depends on previous layout)
# Same tests when we're in non-global fullscreen mode. It should now be possible
# to focus a container in a different workspace.
################################################################################

cmd 'focus parent';
cmd 'fullscreen disable';
cmd 'fullscreen';

cmd '[id="' . $right1->id . '"] focus';
verify_focus($right1, 'upper right window focused');

cmd '[id="' . $right2->id . '"] focus';
verify_focus($right2, 'bottom right window focused');

cmd 'focus parent';
isnt($x->input_focus, $right2->id, 'bottom right window no longer focused');

cmd 'focus child';
verify_focus($right2, 'bottom right window focused again');

cmd 'focus up';
verify_focus($right1, 'allowed focus up');

cmd 'focus down';
verify_focus($right2, 'allowed focus down');

cmd 'focus down';
verify_focus($right1, 'allowed focus wrap (down)');

cmd 'focus up';
verify_focus($right2, 'allowed focus wrap (up)');

cmd 'focus left';
verify_focus($right2, 'focus left wrapped (no-op)');

cmd 'focus right';
verify_focus($diff_ws, 'allowed focus change to different ws');

cmd 'focus left';
verify_focus($right2, 'focused back into fullscreen container');

cmd '[id="' . $diff_ws->id . '"] focus';
verify_focus($diff_ws, 'allowed focus change to different ws by id');

################################################################################
# (depends on previous layout)
# More testing of the interaction between wrapping and the fullscreen focus
# restrictions.
################################################################################

cmd '[id="' . $right1->id . '"] focus';
verify_focus($right1, 'upper right window focused');

cmd 'focus parent';
cmd 'fullscreen disable';
cmd 'focus child';

cmd 'split v';
my $right12 = open_window;

cmd 'focus down';
verify_focus($right2, 'bottom right window focused');

cmd 'split v';
my $right22 = open_window;

cmd 'focus parent';
cmd 'fullscreen';
cmd 'focus child';

cmd 'focus down';
verify_focus($right2, 'focus did not leave parent container (1)');

cmd 'focus down';
verify_focus($right22, 'focus did not leave parent container (2)');

cmd 'focus up';
verify_focus($right2, 'focus did not leave parent container (3)');

cmd 'focus up';
verify_focus($right22, 'focus did not leave parent container (4)');

################################################################################
# (depends on previous layout)
# Ensure that moving in a direction doesn't violate the focus restrictions.
################################################################################

sub verify_move {
    my $num = shift;
    my $msg = shift;
    my $nodes = get_ws_content($tmp2);
    my $split = $nodes->[1];
    my $fs = $split->{nodes}->[1];
    is(scalar @{$fs->{nodes}}, $num, $msg);
}

cmd 'move left';
verify_move(2, 'prevented move left');
cmd 'move right';
verify_move(2, 'prevented move right');
cmd 'move down';
verify_move(2, 'prevented move down');
cmd 'move up';
cmd 'move up';
verify_move(2, 'prevented move up');

################################################################################
# (depends on previous layout)
# Moving to a different workspace is allowed with per-output fullscreen
# containers.
################################################################################

cmd "move to workspace $tmp";
verify_move(1, 'did not prevent move to workspace by name');

cmd "workspace $tmp";
cmd "move to workspace $tmp2";
cmd "workspace $tmp2";

cmd "move to workspace prev";
verify_move(1, 'did not prevent move to workspace by position');

################################################################################
# (depends on previous layout)
# Ensure that is not allowed with global fullscreen containers.
################################################################################

cmd "workspace $tmp";
cmd "move to workspace $tmp2";
cmd "workspace $tmp2";

cmd 'focus parent';
cmd 'fullscreen disable';
cmd 'fullscreen global';
cmd 'focus child';

cmd "move to workspace $tmp";
verify_move(2, 'prevented move to workspace by name');

cmd "move to workspace prev";
verify_move(2, 'prevented move to workspace by position');

kill_all_windows;

################################################################################
# Ensure it's possible to focus a window using the focus command despite
# fullscreen window blocking it. Fullscreen window should lose its fullscreen
# mode.
################################################################################

# first & second tiling, focus using id
$tmp = fresh_workspace;
my $first = open_window;
my $second = open_window;
cmd 'fullscreen';
verify_focus($second, 'fullscreen window focused');
is_num_fullscreen($tmp, 1, '1 fullscreen window');

cmd '[id="' . $first->id . '"] focus';
sync_with_i3;

verify_focus($first, 'correctly focused using id');
is_num_fullscreen($tmp, 0, 'no fullscreen windows');

kill_all_windows;

# first floating, second tiling, focus using 'focus floating'
$tmp = fresh_workspace;
$first = open_floating_window;
$second = open_window;
cmd 'fullscreen';
verify_focus($second, 'fullscreen window focused');
is_num_fullscreen($tmp, 1, '1 fullscreen window');

cmd 'focus floating';
sync_with_i3;

verify_focus($first, 'correctly focused using focus floating');
is_num_fullscreen($tmp, 0, 'no fullscreen windows');

kill_all_windows;

# first tiling, second floating, focus using 'focus tiling'
$tmp = fresh_workspace;
$first = open_window;
$second = open_floating_window;
cmd 'fullscreen';
verify_focus($second, 'fullscreen window focused');
is_num_fullscreen($tmp, 1, '1 fullscreen window');

cmd 'focus tiling';
sync_with_i3;

verify_focus($first, 'correctly focused using focus tiling');
is_num_fullscreen($tmp, 0, 'no fullscreen windows');

kill_all_windows;

################################################################################
# When the fullscreen window is in an other workspace it should maintain its
# fullscreen mode since it's not blocking the window to be focused.
################################################################################

$tmp = fresh_workspace;
$first = open_window;

$tmp2 = fresh_workspace;
$second = open_window;
cmd 'fullscreen';
verify_focus($second, 'fullscreen window focused');
is_num_fullscreen($tmp2, 1, '1 fullscreen window');

cmd '[id="' . $first->id . '"] focus';
sync_with_i3;

verify_focus($first, 'correctly focused using focus id');
is_num_fullscreen($tmp, 0, 'no fullscreen windows on first workspace');
is_num_fullscreen($tmp2, 1, 'still one fullscreen window on second workspace');

kill_all_windows;

################################################################################
# But a global window in another workspace is blocking the window to be focused.
# Ensure that it loses its fullscreen mode.
################################################################################

$tmp = fresh_workspace;
$first = open_window;

$tmp2 = fresh_workspace;
$second = open_window;
cmd 'fullscreen global';
verify_focus($second, 'global window focused');
is_num_fullscreen($tmp2, 1, '1 fullscreen window');

cmd '[id="' . $first->id . '"] focus';
sync_with_i3;

verify_focus($first, 'correctly focused using focus id');
is_num_fullscreen($tmp2, 0, 'no fullscreen windows');


# TODO: Tests for "move to output" and "move workspace to output".
done_testing;
