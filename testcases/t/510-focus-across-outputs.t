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
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Tests that switching workspaces via 'focus $dir' never leaves a floating
# window focused.
#
use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0,1024x768+0+768,1024x768+1024+768
EOT

my $s0_ws = fresh_workspace;
my $first = open_window;
my $second = open_window;
my $third = open_window;
cmd 'floating toggle';

# Focus screen 1
sync_with_i3;
$x->root->warp_pointer(1025, 0);
sync_with_i3;
my $s1_ws = fresh_workspace;

my $fourth = open_window;

# Focus screen 2
sync_with_i3;
$x->root->warp_pointer(0, 769);
sync_with_i3;
my $s2_ws = fresh_workspace;

my $fifth = open_window;

# Focus screen 3
sync_with_i3;
$x->root->warp_pointer(1025, 769);
sync_with_i3;
my $s3_ws = fresh_workspace;

my $sixth = open_window;
my $seventh = open_window;
my $eighth = open_window;
cmd 'floating toggle';

# Layout should look like this (windows 3 and 8 are floating):
#     S0      S1
# ┌───┬───┬───────┐
# │ ┌─┴─┐ │       │
# │1│ 3 │2│   4   │
# │ └─┬─┘ │       │
# ├───┴───┼───┬───┤
# │       │ ┌─┴─┐ │
# │   5   │6│ 8 │7│
# │       │ └─┬─┘ │
# └───────┴───┴───┘
#    S2       S3
#
###################################################################
# Test that focus (left|down|right|up) doesn't focus floating
# windows when moving into horizontally-split workspaces.
###################################################################

sub reset_focus {
    my $ws = shift;
    cmd "workspace $ws; focus floating";
}

cmd "workspace $s1_ws";
cmd 'focus left';
is($x->input_focus, $second->id, 'second window focused');
reset_focus $s0_ws;

cmd "workspace $s1_ws";
cmd 'focus down';
is($x->input_focus, $seventh->id, 'seventh window focused');
reset_focus $s3_ws;

cmd "workspace $s2_ws";
cmd 'focus right';
is($x->input_focus, $seventh->id, 'seventh window focused');
reset_focus $s3_ws;

cmd "workspace $s2_ws";
cmd 'focus up';
is($x->input_focus, $second->id, 'second window focused');
reset_focus $s0_ws;

###################################################################
# Put the workspaces on screens 0 and 3 into vertical split mode
# and test focus (left|down|right|up) again.
###################################################################

cmd "workspace $s0_ws";
is($x->input_focus, $third->id, 'third window focused');
cmd 'focus parent';
cmd 'focus parent';
cmd 'split v';
# Focus second or else $first gets to the top of the focus stack.
cmd '[id=' . $second->id . '] focus';
reset_focus $s0_ws;

cmd "workspace $s3_ws";
is($x->input_focus, $eighth->id, 'eighth window focused');
cmd 'focus parent';
cmd 'focus parent';
cmd 'split v';
cmd '[id=' . $sixth->id . '] focus';
reset_focus $s3_ws;

cmd "workspace $s1_ws";
cmd 'focus left';
is($x->input_focus, $second->id, 'second window focused');
reset_focus $s0_ws;

cmd "workspace $s1_ws";
cmd 'focus down';
is($x->input_focus, $sixth->id, 'sixth window focused');
reset_focus $s3_ws;

cmd "workspace $s2_ws";
cmd 'focus right';
is($x->input_focus, $sixth->id, 'sixth window focused');

cmd "workspace $s2_ws";
cmd 'focus up';
is($x->input_focus, $second->id, 'second window focused');

###################################################################
# Test that focus (left|down|right|up), when focusing across
# outputs, doesn't focus the next window in the given direction but
# the most focused window of the container in the given direction.
# In the following layout:
# [ WS1*[ ] WS2[ H[ A B* ] ] ]
# (where the asterisk denotes the focused container within its
# parent) moving right from WS1 should focus B which is focused
# inside WS2, not A which is the next window on the right of WS1.
# See issue #1160.
###################################################################

kill_all_windows;

sync_with_i3;
$x->root->warp_pointer(1025, 0);  # Second screen.
sync_with_i3;
$s1_ws = fresh_workspace;
$first = open_window;
$second = open_window;

sync_with_i3;
$x->root->warp_pointer(0, 0);  # First screen.
sync_with_i3;
$s0_ws = fresh_workspace;
open_window;
$third = open_window;

cmd 'focus right';
is($x->input_focus, $second->id, 'second window (rightmost) focused');
cmd 'focus left';
is($x->input_focus, $first->id, 'first window focused');
cmd 'focus left';
is($x->input_focus, $third->id, 'third window focused');


###################################################################
# Similar but with a tabbed layout.
###################################################################

cmd 'layout tabbed';
$fourth = open_window;
cmd 'focus left';
is($x->input_focus, $third->id, 'third window (tabbed) focused');
cmd "workspace $s1_ws";
cmd 'focus left';
is($x->input_focus, $third->id, 'third window (tabbed) focused');


###################################################################
# Similar but with a stacked layout on the bottom screen.
###################################################################

sync_with_i3;
$x->root->warp_pointer(0, 769);  # Third screen.
sync_with_i3;
$s2_ws = fresh_workspace;
cmd 'layout stacked';
$fifth = open_window;
$sixth = open_window;

cmd "workspace $s0_ws";
cmd 'focus down';
is($x->input_focus, $sixth->id, 'sixth window (stacked) focused');

###################################################################
# Similar but with a more complex layout.
###################################################################

sync_with_i3;
$x->root->warp_pointer(1025, 769);  # Fourth screen.
sync_with_i3;
$s3_ws = fresh_workspace;
open_window;
open_window;
cmd 'split v';
open_window;
open_window;
cmd 'split h';
my $nested = open_window;
open_window;
cmd 'focus left';
is($x->input_focus, $nested->id, 'nested window focused');

cmd "workspace $s1_ws";
cmd 'focus down';
is($x->input_focus, $nested->id, 'nested window focused from workspace above');

cmd "workspace $s2_ws";
cmd 'focus right';
is($x->input_focus, $nested->id, 'nested window focused from workspace on the left');

done_testing;
