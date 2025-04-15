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
# Test the 'no_focus' directive.
# Ticket: #1416
use i3test i3_autostart => 0;

my ($config, $pid, $ws, $first, $second, $focused);

#####################################################################
# 1: open a window and check that it takes focus
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

$pid = launch_with_config($config);

$ws = fresh_workspace;
$first = open_window;
$focused = get_focused($ws);
$second = open_window;

sync_with_i3;
isnt(get_focused($ws), $focused, 'focus has changed');
is($x->input_focus, $second->id, 'input focus has changed');

exit_gracefully($pid);

#####################################################################
# 2: open a window matched by a no_focus directive and check that
#    it doesn't take focus
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

no_focus [instance=notme]
EOT

$pid = launch_with_config($config);

$ws = fresh_workspace;
$first = open_window;
$focused = get_focused($ws);
$second = open_window(wm_class => 'notme');

sync_with_i3;
is(get_focused($ws), $focused, 'focus has not changed');
is($x->input_focus, $first->id, 'input focus has not changed');

exit_gracefully($pid);

#####################################################################
# 3: no_focus doesn't affect the first window opened on a workspace
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

no_focus [instance=focusme]
EOT

$pid = launch_with_config($config);

$ws = fresh_workspace;
$focused = get_focused($ws);
$first = open_window(wm_class => 'focusme');

sync_with_i3;
is($x->input_focus, $first->id, 'input focus has changed');

# Also check that it counts floating windows
# See issue #3423.
open_floating_window(wm_class => 'focusme');

sync_with_i3;
is($x->input_focus, $first->id, 'input focus didn\'t change to floating window');

exit_gracefully($pid);

#####################################################################

done_testing;
