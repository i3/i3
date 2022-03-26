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
# Verifies that focus output right works with monitor setups that don’t line up
# on their x/y coordinates.
#
# ticket #771, bug still present in commit dd743f3b55b2f86d9f1f69ef7952ae8ece4de504
#
use i3test i3_autostart => 0;

sub test_focus_left_right {
    my ($config) = @_;

    my $pid = launch_with_config($config);

    my $i3 = i3(get_socket_path(0));

    sync_with_i3;
    $x->root->warp_pointer(0, 0);
    sync_with_i3;

    ############################################################################
    # Ensure that moving left and right works.
    ############################################################################

    # First ensure both workspaces have something to focus
    cmd "workspace 1";
    my $left_win = open_window;

    cmd "workspace 2";
    my $right_win = open_window;

    is($x->input_focus, $right_win->id, 'right window focused');

    cmd "focus output left";
    is($x->input_focus, $left_win->id, 'left window focused');

    cmd "focus output right";
    is($x->input_focus, $right_win->id, 'right window focused');

    cmd "focus output right";
    is($x->input_focus, $left_win->id, 'left window focused (wrapping)');

    cmd "focus output left";
    is($x->input_focus, $right_win->id, 'right window focused (wrapping)');

    ############################################################################
    # Ensure that moving down/up from S0 doesn’t crash i3 and is a no-op.
    ############################################################################

    my $second = fresh_workspace(output => 1);
    my $third_win = open_window;

    cmd "focus output down";
    is($x->input_focus, $third_win->id, 'right window still focused');

    cmd "focus output up";
    is($x->input_focus, $third_win->id, 'right window still focused');

    does_i3_live;

    exit_gracefully($pid);
}

# Screen setup looks like this:
# +----+
# |    |--------+
# | S1 |   S2   |
# |    |--------+
# +----+
#
my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1080x1920+0+0,1920x1080+1080+500
EOT

test_focus_left_right($config);

# Screen setup looks like this:
# +----+--------+
# |    |   S2   |
# | S1 |--------+
# |    |
# +----+
#
$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1080x1920+0+0,1920x200+1080+0
EOT

test_focus_left_right($config);

# Screen setup looks like this:
# +----+
# |    |
# | S1 |--------+
# |    |   S2   |
# +----+--------+
#
$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1080x1920+0+0,1920x200+1080+1720
EOT

test_focus_left_right($config);

# Screen setup looks like this:
# +----+        +----+
# |    |        |    |
# | S1 |--------+ S3 |
# |    |   S2   |    |
# +----+--------+----+
#
$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1080x1920+0+0,1920x200+1080+1720,1080x1920+1280+0
EOT

my $pid = launch_with_config($config);

my $i3 = i3(get_socket_path(0));

sync_with_i3;
$x->root->warp_pointer(0, 0);
sync_with_i3;

############################################################################
# Ensure that focusing right/left works in the expected order.
############################################################################

is(focused_output, 'fake-0', 'focus on fake-0');

cmd 'focus output right';
is(focused_output, 'fake-1', 'focus on fake-1');

cmd 'focus output right';
is(focused_output, 'fake-2', 'focus on fake-2');

cmd 'focus output left';
is(focused_output, 'fake-1', 'focus on fake-1');

cmd 'focus output left';
is(focused_output, 'fake-0', 'focus on fake-0');

cmd 'focus output left';
is(focused_output, 'fake-2', 'focus on fake-2 (wrapping)');

cmd 'focus output right';
is(focused_output, 'fake-0', 'focus on fake-0 (wrapping)');

exit_gracefully($pid);

done_testing;
