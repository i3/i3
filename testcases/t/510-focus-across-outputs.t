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
is($x->input_focus, $sixth->id, 'sixth window focused');
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
reset_focus $s0_ws;

cmd "workspace $s3_ws";
is($x->input_focus, $eighth->id, 'eighth window focused');
cmd 'focus parent';
cmd 'focus parent';
cmd 'split v';
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

done_testing;
