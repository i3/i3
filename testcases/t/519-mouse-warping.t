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
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)

use i3test i3_autostart => 0;

# Ensure the pointer is at (0, 0) so that we really start on the first
# (the left) workspace.
$x->root->warp_pointer(0, 0);

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
fake-outputs 1024x768+0+0,1024x768+1024+0
mouse_warping none
EOT
my $pid = launch_with_config($config);

my $i3 = i3(get_socket_path());

######################################################
# Open one workspace with one window on both outputs #
######################################################

# Open window on workspace 1, left output
is(focused_ws, '1', 'starting with focus on workspace 1');
open_window;

# Open window on workspace 2, right output
cmd 'focus output right';
is(focused_ws, '2', 'moved focus to workspace 2');
open_window;

# If mouse_warping is disabled, the pointer has not moved from
# position (0, 0) when focus was switched to workspace 2
$x->root->warp_pointer(0, 0);

# Ensure focus is still on workspace 2
is(focused_ws, '2', 'warped mouse cursor to (0, 0), focus still in workspace 2');

# Exit gracefully
exit_gracefully($pid);
done_testing;
