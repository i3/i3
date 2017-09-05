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
# Tests the swap command.
# Ticket: #917
use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

for_window[class="mark_A"] mark A
for_window[class="mark_B"] mark B
EOT

my ($pid);
my ($ws, $ws1, $ws2, $ws3);
my ($nodes, $expected_focus, $A, $B, $F);
my ($result);
my @urgent;

###############################################################################
# Invalid con_id should not crash i3
# See issue #2895.
###############################################################################

$pid = launch_with_config($config);
$ws = fresh_workspace;

open_window;
cmd "swap container with con_id 1";

does_i3_live;
exit_gracefully($pid);

###############################################################################
# Swap 2 windows in different workspaces using con_id
###############################################################################

$pid = launch_with_config($config);

$ws = fresh_workspace;
open_window;
$A = get_focused($ws);

$ws = fresh_workspace;
open_window;

cmd "swap container with con_id $A";
is(get_focused($ws), $A, 'A is now focused');

exit_gracefully($pid);

###############################################################################
# Swap two containers next to each other.
# Focus should stay on B because both windows are on the focused workspace.
# The focused container is B.
#
# +---+---+    Layout: H1[ A B ]
# | A | B |    Focus Stacks:
# +---+---+        H1: B, A
###############################################################################
$pid = launch_with_config($config);
$ws = fresh_workspace;

$A = open_window(wm_class => 'mark_A');
$B = open_window(wm_class => 'mark_B');
$expected_focus = get_focused($ws);

cmd '[con_mark=B] swap container with mark A';

$nodes = get_ws_content($ws);
is($nodes->[0]->{window}, $B->{id}, 'B is on the left');
is($nodes->[1]->{window}, $A->{id}, 'A is on the right');
is(get_focused($ws), $expected_focus, 'B is still focused');

exit_gracefully($pid);

###############################################################################
# Swap two containers with different parents.
# In this test, the focus head of the left v-split container is A.
# The focused container is B.
#
# +---+---+    Layout: H1[ V1[ A Y ] V2[ X B ] ]
# | A | X |    Focus Stacks:
# +---+---+        H1: V2, V1
# | Y | B |        V1: A, Y
# +---+---+        V2: B, X
###############################################################################
$pid = launch_with_config($config);
$ws = fresh_workspace;

$A = open_window(wm_class => 'mark_A');
$B = open_window(wm_class => 'mark_B');
cmd 'split v';
open_window;
cmd 'move up, focus left';
cmd 'split v';
open_window;
cmd 'focus up, focus right, focus down';
$expected_focus = get_focused($ws);

cmd '[con_mark=B] swap container with mark A';

$nodes = get_ws_content($ws);
is($nodes->[0]->{nodes}->[0]->{window}, $B->{id}, 'B is on the top left');
is($nodes->[1]->{nodes}->[1]->{window}, $A->{id}, 'A is on the bottom right');
is(get_focused($ws), $expected_focus, 'B is still focused');

exit_gracefully($pid);

###############################################################################
# Swap two containers with different parents.
# In this test, the focus head of the left v-split container is _not_ A.
# The focused container is B.
#
# +---+---+    Layout: H1[ V1[ A Y ] V2[ X B ] ]
# | A | X |    Focus Stacks:
# +---+---+        H1: V2, V1
# | Y | B |        V1: Y, A
# +---+---+        V2: B, X
###############################################################################
$pid = launch_with_config($config);
$ws = fresh_workspace;

$A = open_window(wm_class => 'mark_A');
$B = open_window(wm_class => 'mark_B');
cmd 'split v';
open_window;
cmd 'move up, focus left';
cmd 'split v';
open_window;
cmd 'focus right, focus down';
$expected_focus = get_focused($ws);

cmd '[con_mark=B] swap container with mark A';

$nodes = get_ws_content($ws);
is($nodes->[0]->{nodes}->[0]->{window}, $B->{id}, 'B is on the top left');
is($nodes->[1]->{nodes}->[1]->{window}, $A->{id}, 'A is on the bottom right');
is(get_focused($ws), $expected_focus, 'B is still focused');

exit_gracefully($pid);

###############################################################################
# Swap two containers with one being on a different workspace.
# The focused container is B.
#
# Layout: O1[ W1[ H1 ] W2[ H2 ] ]
# Focus Stacks:
#     O1: W2, W1
#
# +---+---+    Layout: H1[ A X ]
# | A | X |    Focus Stacks:
# +---+---+        H1: A, X
#
# +---+---+    Layout: H2[ Y, B ]
# | Y | B |    Focus Stacks:
# +---+---+        H2: B, Y
###############################################################################
$pid = launch_with_config($config);

$ws1 = fresh_workspace;
$A = open_window(wm_class => 'mark_A');
$expected_focus = get_focused($ws1);
open_window;
cmd 'focus left';

$ws2 = fresh_workspace;
open_window;
$B = open_window(wm_class => 'mark_B');

cmd '[con_mark=B] swap container with mark A';

$nodes = get_ws_content($ws1);
is($nodes->[0]->{window}, $B->{id}, 'B is on ws1:left');

$nodes = get_ws_content($ws2);
is($nodes->[1]->{window}, $A->{id}, 'A is on ws1:right');
is(get_focused($ws2), $expected_focus, 'A is focused');

exit_gracefully($pid);

###############################################################################
# Swap two non-focused containers within the same workspace.
#
# +---+---+    Layout: H1[ V1[ A X ] V2[ F B ] ]
# | A | F |    Focus Stacks:
# +---+---+        H1: V2, V1
# | X | B |        V1: A, X
# +---+---+        V2: F, B
###############################################################################
$pid = launch_with_config($config);
$ws = fresh_workspace;

$A = open_window(wm_class => 'mark_A');
$B = open_window(wm_class => 'mark_B');
cmd 'split v';
open_window;
cmd 'move up, focus left';
cmd 'split v';
open_window;
cmd 'focus up, focus right';
$expected_focus = get_focused($ws);

cmd '[con_mark=B] swap container with mark A';

$nodes = get_ws_content($ws);
is($nodes->[0]->{nodes}->[0]->{window}, $B->{id}, 'B is on the top left');
is($nodes->[1]->{nodes}->[1]->{window}, $A->{id}, 'A is on the bottom right');
is(get_focused($ws), $expected_focus, 'F is still focused');

exit_gracefully($pid);

###############################################################################
# Swap two non-focused containers which are both on different workspaces.
#
# Layout: O1[ W1[ A ] W2[ B ] W3[ F ] ]
# Focus Stacks:
#     O1: W3, W2, W1
#
# +---+
# | A |
# +---+
#
# +---+
# | B |
# +---+
#
# +---+
# | F |
# +---+
###############################################################################
$pid = launch_with_config($config);

$ws1 = fresh_workspace;
$A = open_window(wm_class => 'mark_A');

$ws2 = fresh_workspace;
$B = open_window(wm_class => 'mark_B');

$ws3 = fresh_workspace;
open_window;
$expected_focus = get_focused($ws3);

cmd '[con_mark=B] swap container with mark A';

$nodes = get_ws_content($ws1);
is($nodes->[0]->{window}, $B->{id}, 'B is on the first workspace');

$nodes = get_ws_content($ws2);
is($nodes->[0]->{window}, $A->{id}, 'A is on the second workspace');

is(get_focused($ws3), $expected_focus, 'F is still focused');

exit_gracefully($pid);

###############################################################################
# Swap two non-focused containers with one being on a different workspace.
#
# Layout: O1[ W1[ A ] W2[ H2 ] ]
# Focus Stacks:
#     O1: W2, W1
#
# +---+
# | A |
# +---+
#
# +---+---+    Layout: H2[ B, F ]
# | B | F |    Focus Stacks:
# +---+---+        H2: F, B
###############################################################################
$pid = launch_with_config($config);

$ws1 = fresh_workspace;
$A = open_window(wm_class => 'mark_A');

$ws2 = fresh_workspace;
$B = open_window(wm_class => 'mark_B');
open_window;
$expected_focus = get_focused($ws2);

cmd '[con_mark=B] swap container with mark A';

$nodes = get_ws_content($ws1);
is($nodes->[0]->{window}, $B->{id}, 'B is on the first workspace');

$nodes = get_ws_content($ws2);
is($nodes->[0]->{window}, $A->{id}, 'A is on the left of the second workspace');
is(get_focused($ws2), $expected_focus, 'F is still focused');

exit_gracefully($pid);

###############################################################################
# 1. A container cannot be swapped with its parent.
# 2. A container cannot be swapped with one of its children.
#
#      ↓A↓
# +---+---+    Layout: H1[ X V1[ Y B ] ]
# |   | Y |        (with A := V1)
# | X +---+
# |   | B |
# +---+---+
###############################################################################
$pid = launch_with_config($config);

$ws = fresh_workspace;
open_window;
open_window;
cmd 'split v';
$B = open_window(wm_class => 'mark_B');
cmd 'focus parent, mark A, focus child';

$result = cmd '[con_mark=B] swap container with mark A';
is($result->[0]->{success}, 0, 'B cannot be swappd with its parent');

$result = cmd '[con_mark=A] swap container with mark B';
is($result->[0]->{success}, 0, 'A cannot be swappd with one of its children');

exit_gracefully($pid);

###############################################################################
# Swapping two containers preserves the geometry of the container they are
# being swapped with.
#
# Before:
# +---+-------+
# | A |   B   |
# +---+-------+
#
# After:
# +---+-------+
# | B |   A   |
# +---+-------+
###############################################################################
$pid = launch_with_config($config);

$ws = fresh_workspace;
$A = open_window(wm_class => 'mark_A');
$B = open_window(wm_class => 'mark_B');
cmd 'resize grow width 0 or 25 ppt';

# sanity checks
$nodes = get_ws_content($ws);
cmp_float($nodes->[0]->{percent}, 0.25, 'A has 25% width');
cmp_float($nodes->[1]->{percent}, 0.75, 'B has 75% width');

cmd '[con_mark=B] swap container with mark A';

$nodes = get_ws_content($ws);
cmp_float($nodes->[0]->{percent}, 0.25, 'B has 25% width');
cmp_float($nodes->[1]->{percent}, 0.75, 'A has 75% width');

exit_gracefully($pid);

###############################################################################
# Swapping containers not sharing the same parent preserves the geometry of
# the container they are swapped with.
#
# Before:
# +---+-----+
# | A |     |
# +---+  B  |
# |   |     |
# | Y +-----+
# |   |  X  |
# +---+-----+
#
# After:
# +---+-----+
# | B |     |
# +---+  A  |
# |   |     |
# | Y +-----+
# |   |  X  |
# +---+-----+
###############################################################################
$pid = launch_with_config($config);
$ws = fresh_workspace;

$A = open_window(wm_class => 'mark_A');
$B = open_window(wm_class => 'mark_B');
cmd 'split v';
open_window;
cmd 'focus up, resize grow height 0 or 25 ppt';
cmd 'focus left, split v';
open_window;
cmd 'resize grow height 0 or 25 ppt';

# sanity checks
$nodes = get_ws_content($ws);
cmp_float($nodes->[0]->{nodes}->[0]->{percent}, 0.25, 'A has 25% height');
cmp_float($nodes->[1]->{nodes}->[0]->{percent}, 0.75, 'B has 75% height');

cmd '[con_mark=B] swap container with mark A';

$nodes = get_ws_content($ws);
cmp_float($nodes->[0]->{nodes}->[0]->{percent}, 0.25, 'B has 25% height');
cmp_float($nodes->[1]->{nodes}->[0]->{percent}, 0.75, 'A has 75% height');

exit_gracefully($pid);

###############################################################################
# Swapping containers moves the urgency hint correctly.
###############################################################################
$pid = launch_with_config($config);

$ws1 = fresh_workspace;
$A = open_window(wm_class => 'mark_A');
$ws2 = fresh_workspace;
$B = open_window(wm_class => 'mark_B');
open_window;

$B->add_hint('urgency');
sync_with_i3;

cmd '[con_mark=B] swap container with mark A';

@urgent = grep { $_->{urgent} } @{get_ws_content($ws1)};
is(@urgent, 1, 'B is marked urgent');
is(get_ws($ws1)->{urgent}, 1, 'the first workspace is marked urgent');

@urgent = grep { $_->{urgent} } @{get_ws_content($ws2)};
is(@urgent, 0, 'A is not marked urgent');
is(get_ws($ws2)->{urgent}, 0, 'the second workspace is not marked urgent');

exit_gracefully($pid);

###############################################################################

done_testing;
