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
# Tests if a simple 'move <direction>' command will move containers across outputs.
#
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0,1024x768+1024+768,1024x768+0+768

workspace left-top output fake-0
workspace right-top output fake-1
workspace right-bottom output fake-2
workspace left-bottom output fake-3
EOT

# Ensure the pointer is at (0, 0) so that we really start on the first
# (the left) workspace.
$x->root->warp_pointer(0, 0);

#####################################################################
# Try to move a single window across outputs in each direction
#####################################################################

cmd('workspace left-top');
my $alone_window = open_window;

cmd('move right');
is(scalar @{get_ws_content('right-top')}, 1, 'moved individual window to right-top workspace');

cmd('move down');
is(scalar @{get_ws_content('right-bottom')}, 1, 'moved individual window to right-bottom workspace');

cmd('move left');
is(scalar @{get_ws_content('left-bottom')}, 1, 'moved individual window to left-bottom workspace');

cmd('move up');
is(scalar @{get_ws_content('left-top')}, 1, 'moved individual window to left-top workspace');

$alone_window->unmap;
wait_for_unmap;

#####################################################################
# Try to move a window on a workspace with two windows across outputs in each
# direction
#####################################################################

# from left-top to right-top
cmd('workspace left-top');
cmd('split h');
my $first_window = open_window;
my $social_window = open_window( name => 'CORRECT_WINDOW' );
cmd('move right');
is(scalar @{get_ws_content('right-top')}, 1, 'moved some window to right-top workspace');
my $compare_window = shift @{get_ws_content('right-top')};
is($compare_window->{window}, $social_window->id, 'moved correct window to right-top workspace');
# unmap the first window so we don't confuse it when we move back here
$first_window->unmap;
wait_for_unmap;

# from right-top to right-bottom
cmd('split v');
open_window;
# this window opened above - we need to move down twice
cmd('focus up; move down; move down');
is(scalar @{get_ws_content('right-bottom')}, 1, 'moved some window to right-bottom workspace');
$compare_window = shift @{get_ws_content('right-bottom')};
is($compare_window->{name}, $social_window->name, 'moved correct window to right-bottom workspace');

# from right-bottom to left-bottom
cmd('split h');
open_window;
cmd('focus left; move left');
is(scalar @{get_ws_content('left-bottom')}, 1, 'moved some window to left-bottom workspace');
$compare_window = shift @{get_ws_content('left-bottom')};
is($social_window->name, $compare_window->{name}, 'moved correct window to left-bottom workspace');

# from left-bottom to left-top
cmd('split v');
open_window;
cmd('focus up; move up');
is(scalar @{get_ws_content('left-top')}, 1, 'moved some window to left-bottom workspace');
$compare_window = shift @{get_ws_content('left-top')};
is($social_window->name, $compare_window->{name}, 'moved correct window to left-bottom workspace');

#####################################################################
# Moving a fullscreen container should change its output.
#####################################################################

kill_all_windows;

cmd 'workspace left-top';
open_window;
my $fs_window = open_window;
open_window;

cmd '[id=' . $fs_window->id . '] fullscreen enable, move right';
is(scalar @{get_ws_content('right-top')}, 1, 'moved fullscreen window to right-top workspace');

#####################################################################
# Moving a global fullscreen container should not change its output.
#####################################################################

kill_all_windows;

cmd 'workspace left-top';
open_window;
open_window;
open_window;

cmd 'fullscreen global, move right, fullscreen disable';
is(scalar @{get_ws_content('right-top')}, 0, 'global fullscreen window didn\'t change workspace with move');

done_testing;
