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
# Tests whether 'workspace next_on_output' and the like work correctly.
#
use List::Util qw(first);
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

################################################################################
# Setup workspaces so that they stay open (with an empty container).
################################################################################

sync_with_i3;
$x->root->warp_pointer(0, 0);
sync_with_i3;

is(focused_ws, '1', 'starting on workspace 1');
# ensure workspace 1 stays open
open_window;

cmd 'focus output right';
is(focused_ws, '2', 'workspace 2 on second output');
# ensure workspace 2 stays open
open_window;

cmd 'workspace 6:c';
# ensure workspace 6:c stays open
open_window;

cmd 'focus output right';
is(focused_ws, '1', 'back on workspace 1');

# We don’t use fresh_workspace with named workspaces here since they come last
# when using 'workspace next'.
cmd 'workspace 5';
# ensure workspace 5 stays open
open_window;

cmd 'workspace 6:a';
# ensure workspace 6:a stays open
open_window;

cmd 'workspace 6:b';
# ensure workspace 6:b stays open
open_window;

################################################################################
# Use workspace next and verify the correct order.
################################################################################

# The current order should be:
# output 1: 1, 5, 6:a, 6:b
# output 2: 2, 6:c
cmd 'workspace 1';
cmd 'workspace next';
# We need to sync after changing focus to a different output to wait for the
# EnterNotify to be processed, otherwise it will be processed at some point
# later in time and mess up our subsequent tests.
sync_with_i3;

is(focused_ws, '2', 'workspace 2 focused');
cmd 'workspace next';
# We need to sync after changing focus to a different output to wait for the
# EnterNotify to be processed, otherwise it will be processed at some point
# later in time and mess up our subsequent tests.
sync_with_i3;

is(focused_ws, '5', 'workspace 5 focused');
cmd 'workspace next';
# We need to sync after changing focus to a different output to wait for the
# EnterNotify to be processed, otherwise it will be processed at some point
# later in time and mess up our subsequent tests.
sync_with_i3;

is(focused_ws, '6:a', 'workspace 6:a focused');
cmd 'workspace next';
# We need to sync after changing focus to a different output to wait for the
# EnterNotify to be processed, otherwise it will be processed at some point
# later in time and mess up our subsequent tests.
sync_with_i3;

is(focused_ws, '6:b', 'workspace 6:a focused');
cmd 'workspace next';
# We need to sync after changing focus to a different output to wait for the
# EnterNotify to be processed, otherwise it will be processed at some point
# later in time and mess up our subsequent tests.
sync_with_i3;

is(focused_ws, '6:c', 'workspace 6:b focused');

################################################################################
# Now try the same with workspace next_on_output.
################################################################################

cmd 'workspace 1';
cmd 'workspace next_on_output';
is(focused_ws, '5', 'workspace 5 focused');
cmd 'workspace next_on_output';
is(focused_ws, '6:a', 'workspace 6:a focused');
cmd 'workspace next_on_output';
is(focused_ws, '6:b', 'workspace 6:b focused');
cmd 'workspace next_on_output';
is(focused_ws, '1', 'workspace 1 focused');

cmd 'workspace prev_on_output';
is(focused_ws, '6:b', 'workspace 6:b focused');
cmd 'workspace prev_on_output';
is(focused_ws, '6:a', 'workspace 6:a focused');
cmd 'workspace prev_on_output';
is(focused_ws, '5', 'workspace 5 focused');
cmd 'workspace prev_on_output';
is(focused_ws, '1', 'workspace 1 focused');

cmd 'workspace 2';
# We need to sync after changing focus to a different output to wait for the
# EnterNotify to be processed, otherwise it will be processed at some point
# later in time and mess up our subsequent tests.
sync_with_i3;

cmd 'workspace prev_on_output';
is(focused_ws, '6:c', 'workspace 6:c focused');

cmd 'workspace prev_on_output';
is(focused_ws, '2', 'workspace 2 focused');

done_testing;
