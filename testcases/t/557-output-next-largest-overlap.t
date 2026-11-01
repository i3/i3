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
# When several outputs are equally close in the requested direction, prefer
# the one that shares the longest edge with the current output.
#
# +-----------------++-----------------+
# |    fake-0       ||      top        |
# |   1920x1200     ||   1920x1080     |
# |                 |+-----------------+
# +-----------------++-----------------+
#                    |     bottom      |
#                    |   1920x1080     |
#                    +-----------------+
#
# Ticket: #4079
# Bug still in: 4.25-29-gbe04c853
use i3test i3_autostart => 0;

# The bug depended on the order in which outputs are iterated.
for my $order (0, 1) {
    my ($top, $bottom) = $order ? ('fake-1', 'fake-2') : ('fake-2', 'fake-1');
    my $outputs = $order
        ? '1920x1200+0+0,1920x1080+1920+0,1920x1080+1920+1080'
        : '1920x1200+0+0,1920x1080+1920+1080,1920x1080+1920+0';

    my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs $outputs
workspace left output fake-0
workspace top output $top
workspace bottom output $bottom
EOT

    my $pid = launch_with_config($config);

    cmd 'workspace top';
    cmd 'workspace bottom';
    cmd 'workspace left';
    is(focused_output, 'fake-0', 'sanity check');

    cmd 'focus output right';
    is(focused_output, $top, 'focus output right picks output with largest shared edge');

    cmd 'workspace left';
    my $w1 = open_window;
    cmd 'move right';
    is_num_children('top', 1, 'move right moves window to output with largest shared edge');
    is_num_children('left', 0, 'window left workspace "left"');

    cmd 'workspace left';
    my $w2 = open_window;
    cmd 'move container to output right';
    is_num_children('top', 2, 'move container to output right uses largest shared edge');

    cmd 'workspace bottom';
    cmd 'focus output left';
    is(focused_output, 'fake-0', 'focus output left from bottom output');

    kill_all_windows;
    exit_gracefully($pid);
}

done_testing;
