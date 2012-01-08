#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests whether 'workspace next_on_output' and the like work correctly.
#
use List::Util qw(first);
use i3test;

################################################################################
# Setup workspaces so that they stay open (with an empty container).
################################################################################

is(focused_ws, '1', 'starting on workspace 1');
# ensure workspace 1 stays open
cmd 'open';

cmd 'focus output right';
is(focused_ws, '2', 'workspace 2 on second output');
# ensure workspace 2 stays open
cmd 'open';

cmd 'focus output right';
is(focused_ws, '1', 'back on workspace 1');

# We don’t use fresh_workspace with named workspaces here since they come last
# when using 'workspace next'.
cmd 'workspace 5';
# ensure workspace $tmp stays open
cmd 'open';

################################################################################
# Use workspace next and verify the correct order.
################################################################################

# The current order should be:
# output 1: 1, 5
# output 2: 2
cmd 'workspace 1';
cmd 'workspace next';
is(focused_ws, '2', 'workspace 2 focused');
cmd 'workspace next';
is(focused_ws, '5', 'workspace 5 focused');

################################################################################
# Now try the same with workspace next_on_output.
################################################################################

cmd 'workspace 1';
cmd 'workspace next_on_output';
is(focused_ws, '5', 'workspace 5 focused');
cmd 'workspace next_on_output';
is(focused_ws, '1', 'workspace 1 focused');

cmd 'workspace prev_on_output';
is(focused_ws, '5', 'workspace 5 focused');
cmd 'workspace prev_on_output';
is(focused_ws, '1', 'workspace 1 focused');

cmd 'workspace 2';

cmd 'workspace prev_on_output';
is(focused_ws, '2', 'workspace 2 focused');

done_testing;
