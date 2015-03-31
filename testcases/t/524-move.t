#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Tests the behaviour of 'move <direction>' when moving containers across
# outputs on workspaces that have non-default layouts.
# Ticket: #1603
# Bug still in: 4.10.1-40-g0ad097e
use List::Util qw(first);
use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0,1024x768+1024+768,1024x768+0+768

workspace left-top output fake-0
workspace right-top output fake-1
workspace right-bottom output fake-2
workspace left-bottom output fake-3

workspace_layout stacked
EOT

my $pid = launch_with_config($config);

#####################################################################
# Create two windows in the upper left workspace and move them
# clockwise around the workspaces until the end up where they began.
#####################################################################

cmd 'workspace left-top';

my $first = open_window(wm_class => 'first');
my $second = open_window(wm_class => 'second');

is_num_children('left-top', 1, 'one child on left-top');
is_num_children('right-top', 0, 'no children on right-top');

# Move the second window into its own stacked container.
cmd 'move right';
is_num_children('left-top', 2, 'two children on left-top');

# Move the second window onto the upper right workspace.
cmd 'move right';
is_num_children('left-top', 1, 'one child on left-top');
is_num_children('right-top', 1, 'one child on right-top');

# Move the first window onto the upper right workspace.
cmd '[class="first"] move right';
is_num_children('left-top', 0, 'no children on left-top');
is_num_children('right-top', 2, 'two children on right-top');

# Move the second window onto the lower right workspace.
cmd '[class="second"] move down, move down';
is_num_children('right-top', 1, 'one child on right-top');
is_num_children('right-bottom', 1, 'two children on right-bottom');

# Move the first window onto the lower right workspace.
cmd '[class="first"] move down';
is_num_children('right-top', 0, 'no children on right-top');
is_num_children('right-bottom', 2, 'two children on right-bottom');

# Move the second windo onto the lower left workspace.
cmd '[class="second"] move left, move left';
is_num_children('right-bottom', 1, 'one child on right-bottom');
is_num_children('left-bottom', 1, 'one on left-bottom');

# Move the first window onto the lower left workspace.
cmd '[class="first"] move left';
is_num_children('right-bottom', 0, 'no children on right-bottom');
is_num_children('left-bottom', 2, 'two children on left-bottom');

# Move the second window onto the upper left workspace.
cmd '[class="second"] move up, move up';
is_num_children('left-bottom', 1, 'one child on left-bottom');
is_num_children('left-top', 1, 'one child on left-top');

# Move the first window onto the upper left workspace.
cmd '[class="first"] move up';
is_num_children('left-bottom', 0, 'no children on left-bottom');
is_num_children('left-top', 2, 'two children on left-top');

exit_gracefully($pid);

done_testing;
