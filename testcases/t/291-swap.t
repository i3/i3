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
# Tests the swap command.
# Ticket: #917
use i3test i3_autostart => 0;

my $config = <<EOT;
font font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

for_window[class="mark_A"] mark A
for_window[class="mark_B"] mark B
EOT
my $pid = launch_with_config($config);

my ($ws, $ws1, $ws2, $ws3);
my ($node, $nodes, $expected_focus, $A, $B, $F);
my ($result);
my @fullscreen_permutations = ([], ["A"], ["B"], ["A", "B"]);
my $rect_A = [ 100, 100, 100, 100 ];
my $rect_B = [ 200, 200, 200, 200 ];
my @urgent;

sub fullscreen_windows {
    my $ws = shift if @_;

    scalar grep { $_->{fullscreen_mode} != 0 } @{get_ws_content($ws)}
}

sub cmp_floating_rect {
    my ($window, $rect, $prefix) = @_;
    sync_with_i3;
    my ($absolute, $top) = $window->rect;

    is($absolute->{width}, $rect->[2], "$prefix: width matches");
    is($absolute->{height}, $rect->[3], "$prefix: height matches");

    is($top->{x}, $rect->[0], "$prefix: x matches");
    is($top->{y}, $rect->[1], "$prefix: y matches");
}

###############################################################################
# Invalid con_id should not crash i3
# See issue #2895.
###############################################################################

$ws = fresh_workspace;

open_window;
cmd "swap container with con_id 1";

does_i3_live;
kill_all_windows;

###############################################################################
# Swap 2 windows in different workspaces using con_id
###############################################################################

$ws = fresh_workspace;
open_window;
$A = get_focused($ws);

$ws = fresh_workspace;
open_window;

cmd "swap container with con_id $A";
is(get_focused($ws), $A, 'A is now focused');

kill_all_windows;

###############################################################################
# Swap two containers next to each other.
# Focus should stay on B because both windows are on the focused workspace.
# The focused container is B.
#
# +---+---+    Layout: H1[ A B ]
# | A | B |    Focus Stacks:
# +---+---+        H1: B, A
###############################################################################
$ws = fresh_workspace;

$A = open_window(wm_class => 'mark_A');
$B = open_window(wm_class => 'mark_B');
$expected_focus = get_focused($ws);

cmd '[con_mark=B] swap container with mark A';

$nodes = get_ws_content($ws);
is($nodes->[0]->{window}, $B->{id}, 'B is on the left');
is($nodes->[1]->{window}, $A->{id}, 'A is on the right');
is(get_focused($ws), $expected_focus, 'B is still focused');

kill_all_windows;

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

kill_all_windows;

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

kill_all_windows;

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
for my $fullscreen (@fullscreen_permutations){
    $ws1 = fresh_workspace;
    $A = open_window(wm_class => 'mark_A');
    $expected_focus = get_focused($ws1);
    open_window;
    cmd 'focus left';

    $ws2 = fresh_workspace;
    open_window;
    $B = open_window(wm_class => 'mark_B');

    my $A_fullscreen = "A" ~~ @$fullscreen || 0;
    my $B_fullscreen = "B" ~~ @$fullscreen || 0;
    $A->fullscreen($A_fullscreen);
    $B->fullscreen($B_fullscreen);
    sync_with_i3;

    cmd '[con_mark=B] swap container with mark A';

    $nodes = get_ws_content($ws1);
    $node = $nodes->[0];
    is($node->{window}, $B->{id}, 'B is on ws1:left');
    is_num_fullscreen($ws1, $A_fullscreen, 'amount of fullscreen windows in ws1');
    is($node->{fullscreen_mode}, $A_fullscreen, 'B got A\'s fullscreen mode');

    $nodes = get_ws_content($ws2);
    $node = $nodes->[1];
    is($node->{window}, $A->{id}, 'A is on ws2:right');
    is(get_focused($ws2), $expected_focus, 'A is focused');
    is_num_fullscreen($ws2, $B_fullscreen, 'amount of fullscreen windows in ws2');
    is($node->{fullscreen_mode}, $B_fullscreen, 'A got B\'s fullscreen mode');

    kill_all_windows;
}

###############################################################################
# Swap a non-fullscreen window with a fullscreen one in different workspaces.
# Layout: O1[ W1[ H1 ] W2[ B ] ]
#
# +---+---+    Layout: H1[ A F ]
# | A | F |    Focus Stacks:
# +---+---+        H1: F, A
#
# +---+---+
# |   B   |
# +---+---+
###############################################################################
$ws1 = fresh_workspace;

$A = open_window(wm_class => 'mark_A');
$F = open_window();
$F->fullscreen(1);
$expected_focus = get_focused($ws1);

$ws2 = fresh_workspace;
$B = open_window(wm_class => 'mark_B');
$B->fullscreen(1);
sync_with_i3;

cmd '[con_mark=B] swap container with mark A';

$nodes = get_ws_content($ws1);
is($nodes->[0]->{window}, $B->{id}, 'B is on ws1:left');
is_num_fullscreen($ws1, 1, 'F still fullscreen in ws1');
is(get_focused($ws1), $expected_focus, 'F is still focused');

$nodes = get_ws_content($ws2);
is($nodes->[0]->{window}, $A->{id}, 'A is on ws1');

###############################################################################
# Try a more exotic layout with fullscreen containers.
# A and F are fullscreened as a stack of two vertical containers before the
# swap is performed.
# A is swapped with fullscreened window B which is in another workspace.
#
# +---+---+    Layout: H1[ X V1[ A F ] ]
# |   | A |    Focus Stacks:
# | X +---+        H1: V1, X
# |   | F |        V1: F, A
# +---+---+
###############################################################################
$ws1 = fresh_workspace;

open_window;
$A = open_window(wm_class => 'mark_A');
cmd "split v";
open_window;
cmd "focus parent";
cmd "fullscreen enable";
$expected_focus = get_focused($ws1);

$ws2 = fresh_workspace;
$B = open_window(wm_class => 'mark_B');
$B->fullscreen(1);
sync_with_i3;

cmd '[con_mark=B] swap container with mark A';

does_i3_live;

$nodes = get_ws_content($ws1);
is($nodes->[1]->{nodes}->[0]->{window}, $B->{id}, 'B is on top right in ws1');
is(get_focused($ws1), $expected_focus, 'The container of the stacked windows remains focused in ws1');
is_num_fullscreen($ws1, 1, 'Same amount of fullscreen windows in ws1');

$nodes = get_ws_content($ws2);
is($nodes->[0]->{window}, $A->{id}, 'A is on ws2');
is_num_fullscreen($ws2, 1, 'A is in fullscreen mode');

###############################################################################
# Swap two non-focused containers within the same workspace.
#
# +---+---+    Layout: H1[ V1[ A X ] V2[ F B ] ]
# | A | F |    Focus Stacks:
# +---+---+        H1: V2, V1
# | X | B |        V1: A, X
# +---+---+        V2: F, B
###############################################################################
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

kill_all_windows;

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
for my $fullscreen (@fullscreen_permutations){
    $ws1 = fresh_workspace;
    $A = open_window(wm_class => 'mark_A');

    $ws2 = fresh_workspace;
    $B = open_window(wm_class => 'mark_B');

    $ws3 = fresh_workspace;
    open_window;
    $expected_focus = get_focused($ws3);

    my $A_fullscreen = "A" ~~ @$fullscreen || 0;
    my $B_fullscreen = "B" ~~ @$fullscreen || 0;
    $A->fullscreen($A_fullscreen);
    $B->fullscreen($B_fullscreen);
    sync_with_i3;

    cmd '[con_mark=B] swap container with mark A';

    $nodes = get_ws_content($ws1);
    $node = $nodes->[0];
    is($node->{window}, $B->{id}, 'B is on the first workspace');
    is_num_fullscreen($ws1, $A_fullscreen, 'amount of fullscreen windows in ws1');
    is($node->{fullscreen_mode}, $A_fullscreen, 'B got A\'s fullscreen mode');

    $nodes = get_ws_content($ws2);
    $node = $nodes->[0];
    is($node->{window}, $A->{id}, 'A is on the second workspace');
    is_num_fullscreen($ws2, $B_fullscreen, 'amount of fullscreen windows in ws2');
    is($node->{fullscreen_mode}, $B_fullscreen, 'A got B\'s fullscreen mode');

    is(get_focused($ws3), $expected_focus, 'F is still focused');

    kill_all_windows;
}

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
#
# See issue: #3259
###############################################################################

for my $fullscreen (0..1){
    $ws1 = fresh_workspace;
    $A = open_window(wm_class => 'mark_A');

    $ws2 = fresh_workspace;
    $B = open_window(wm_class => 'mark_B');
    open_window;
    cmd 'fullscreen enable' if $fullscreen;
    $expected_focus = get_focused($ws2);

    cmd '[con_mark=B] swap container with mark A';

    $nodes = get_ws_content($ws1);
    is($nodes->[0]->{window}, $B->{id}, 'B is on the first workspace');

    $nodes = get_ws_content($ws2);
    is($nodes->[0]->{window}, $A->{id}, 'A is on the left of the second workspace');
    is(get_focused($ws2), $expected_focus, 'F is still focused');

    kill_all_windows;
}

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

kill_all_windows;

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

kill_all_windows;

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

kill_all_windows;

###############################################################################
# Swap floating windows in the same workspace. Check that they exchange rects.
###############################################################################
$ws = fresh_workspace;

$A = open_floating_window(wm_class => 'mark_A', rect => $rect_A);
$B = open_floating_window(wm_class => 'mark_B', rect => $rect_B);

cmd '[con_mark=B] swap container with mark A';
cmp_floating_rect($A, $rect_B, 'A got B\'s rect');
cmp_floating_rect($B, $rect_A, 'B got A\'s rect');

kill_all_windows;

###############################################################################
# Swap a fullscreen floating and a normal floating window.
###############################################################################
$ws1 = fresh_workspace;
$A = open_floating_window(wm_class => 'mark_A', rect => $rect_A, fullscreen => 1);
$ws2 = fresh_workspace;
$B = open_floating_window(wm_class => 'mark_B', rect => $rect_B);

cmd '[con_mark=B] swap container with mark A';

cmp_floating_rect($A, $rect_B, 'A got B\'s rect');

$nodes = get_ws($ws1);
$node = $nodes->{floating_nodes}->[0]->{nodes}->[0];
is($node->{window}, $B->{id}, 'B is on the first workspace');
is($node->{fullscreen_mode}, 1, 'B is now fullscreened');

$nodes = get_ws($ws2);
$node = $nodes->{floating_nodes}->[0]->{nodes}->[0];
is($node->{window}, $A->{id}, 'A is on the second workspace');
is($node->{fullscreen_mode}, 0, 'A is not fullscreened anymore');

kill_all_windows;

###############################################################################
# Swap a floating window which is in a workspace that has another, regular
# window with a regular window in another workspace. A & B focused.
#
# Before:
# +-----------+
# |         F |
# |   +---+   |
# |   | A |   |
# |   +---+   |
# |           |
# +-----------+
#
# +-----------+
# |           |
# |           |
# |     B     |
# |           |
# |           |
# +-----------+
#
# After: Same as above but A <-> B. A & B focused.
###############################################################################
$ws1 = fresh_workspace;
open_window;
$A = open_floating_window(wm_class => 'mark_A', rect => $rect_A);
$ws2 = fresh_workspace;
$B = open_window(wm_class => 'mark_B');
$expected_focus = get_focused($ws2);

cmd '[con_mark=B] swap container with mark A';

$nodes = get_ws($ws1);
$node = $nodes->{floating_nodes}->[0]->{nodes}->[0];
is($node->{window}, $B->{id}, 'B is floating on the first workspace');
is(get_focused($ws1), $expected_focus, 'B is focused');
cmp_floating_rect($B, $rect_A, 'B got A\'s rect');

$nodes = get_ws_content($ws2);
$node = $nodes->[0];
is($node->{window}, $A->{id}, 'A is on the second workspace');

kill_all_windows;

###################################################################
# Test that swapping a floating window maintains the correct
# floating order.
###################################################################
$ws = fresh_workspace;
$A = open_floating_window(wm_class => 'mark_A', rect => $rect_A);
$B = open_window(wm_class => 'mark_B');
$F = open_floating_window;
$expected_focus = get_focused($ws);

cmd '[con_mark=B] swap container with mark A';

$nodes = get_ws($ws);

$node = $nodes->{floating_nodes}->[0]->{nodes}->[0];
is($node->{window}, $B->{id}, 'B is floating, bottom');
cmp_floating_rect($B, $rect_A, 'B got A\'s rect');

$node = $nodes->{floating_nodes}->[1]->{nodes}->[0];
is($node->{window}, $F->{id}, 'F is floating, top');
is(get_focused($ws), $expected_focus, 'F still focused');

$node = $nodes->{nodes}->[0];
is($node->{window}, $A->{id}, 'A is tiling');

kill_all_windows;

###############################################################################
# Swap a sticky, floating container A and a floating fullscreen container B.
# A should remain sticky and floating and should be fullscreened.
###############################################################################
$ws1 = fresh_workspace;
open_window;
$A = open_floating_window(wm_class => 'mark_A', rect => $rect_A);
$expected_focus = get_focused($ws1);
cmd 'sticky enable';
$B = open_floating_window(wm_class => 'mark_B', rect => $rect_B);
cmd 'fullscreen enable';

cmd '[con_mark=B] swap container with mark A';

is(@{get_ws($ws1)->{floating_nodes}}, 2, '2 fullscreen containers on first workspace');
is(get_focused($ws1), $expected_focus, 'A is focused');

$ws2 = fresh_workspace;
cmd 'fullscreen disable';
cmp_floating_rect($A, $rect_B, 'A got B\'s rect');
is(@{get_ws($ws2)->{floating_nodes}}, 1, 'only A in new workspace');

cmd "workspace $ws1"; # TODO: Why does rect check fails without switching workspaces?
cmp_floating_rect($B, $rect_A, 'B got A\'s rect');

kill_all_windows;

###############################################################################
# Swapping containers moves the urgency hint correctly.
###############################################################################

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

kill_all_windows;

###############################################################################
# Swapping an urgent container from another workspace with the focused
# container removes the urgency hint from both the container and the previous
# workspace.
###############################################################################

$ws2 = fresh_workspace;
$B = open_window(wm_class => 'mark_B');
$ws1 = fresh_workspace;
$A = open_window(wm_class => 'mark_A');

$B->add_hint('urgency');
sync_with_i3;

cmd '[con_mark=B] swap container with mark A';

@urgent = grep { $_->{urgent} } @{get_ws_content($ws1)};
is(@urgent, 0, 'B is not marked urgent');
is(get_ws($ws1)->{urgent}, 0, 'the first workspace is not marked urgent');

@urgent = grep { $_->{urgent} } @{get_ws_content($ws2)};
is(@urgent, 0, 'A is not marked urgent');
is(get_ws($ws2)->{urgent}, 0, 'the second workspace is not marked urgent');

kill_all_windows;

exit_gracefully($pid);

###############################################################################
# Test that swapping with workspace_layout doesn't crash i3.
# See issue #3280.
###############################################################################

$config = <<EOT;
font font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

workspace_layout stacking
EOT
$pid = launch_with_config($config);

$ws = fresh_workspace;
open_window;
open_window;
$B = open_window;

cmd 'move right';
cmd 'focus left, focus parent, mark a';
cmd 'focus right';
cmd 'swap with mark a';

does_i3_live;

$nodes = get_ws_content($ws);
is($nodes->[0]->{window}, $B->{id}, 'B is on the left');

exit_gracefully($pid);

###############################################################################

done_testing;
