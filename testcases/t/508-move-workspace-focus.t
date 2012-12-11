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
# Regression test: Verify that focus is correct after moving a floating window
# to a workspace on a different visible output.
# Bug still in: 4.3-83-ge89a25f
use i3test i3_autostart => 0;

# Ensure the pointer is at (0, 0) so that we really start on the first
# (the left) workspace.
$x->root->warp_pointer(0, 0);

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT
my $pid = launch_with_config($config);

my $left_ws = fresh_workspace(output => 0);
open_window;

my $right_ws = fresh_workspace(output => 1);
open_window;
my $right_float = open_floating_window;

cmd "move workspace $left_ws";
is($x->input_focus, $right_float->id, 'floating window still focused');

exit_gracefully($pid);

done_testing;
