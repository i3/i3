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
# Tests resizing tiling containers
use i3test;

############################################################
# resize horizontally
############################################################

my $tmp = fresh_workspace;

cmd 'split h';

my $left = open_window;
my $right = open_window;

diag("left = " . $left->id . ", right = " . $right->id);

is($x->input_focus, $right->id, 'Right window focused');

cmd 'resize set 75 ppt 0 ppt';

my ($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.25, 'left window got only 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'right window got 75%');

# Same but use the 'width' keyword.
cmd 'resize set width 80 ppt';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.20, 'left window got 20%');
cmp_float($nodes->[1]->{percent}, 0.80, 'right window got 80%');

# Same but with px.
cmd 'resize set width 200 px';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[1]->{rect}->{width}, 200, 'right window got 200 px');

############################################################
# resize vertically
############################################################

$tmp = fresh_workspace;

cmd 'split v';

my $top = open_window;
my $bottom = open_window;

diag("top = " . $top->id . ", bottom = " . $bottom->id);

is($x->input_focus, $bottom->id, 'Bottom window focused');

cmd 'resize set 0 ppt 75 ppt';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.25, 'top window got only 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'bottom window got 75%');

# Same but use the 'height' keyword.
cmd 'resize set height 80 ppt';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.20, 'top window got 20%');
cmp_float($nodes->[1]->{percent}, 0.80, 'bottom window got 80%');

# Same but with px.
cmd 'resize set height 200 px';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[1]->{rect}->{height}, 200, 'bottom window got 200 px');

############################################################
# resize horizontally and vertically
############################################################

$tmp = fresh_workspace;

cmd 'split h';
$left = open_window;
my $top_right = open_window;
cmd 'split v';
my $bottom_right = open_window;

diag("left = " . $left->id . ", top-right = " . $top_right->id . ", bottom-right = " . $bottom_right->id);

is($x->input_focus, $bottom_right->id, 'Bottom-right window focused');

cmd 'resize set 75 ppt 75 ppt';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.25, 'left container got 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'right container got 75%');
cmp_float($nodes->[1]->{nodes}->[0]->{percent}, 0.25, 'top-right window got 25%');
cmp_float($nodes->[1]->{nodes}->[1]->{percent}, 0.75, 'bottom-right window got 75%');

# Same but with px.
cmd 'resize set 155 px 135 px';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[1]->{nodes}->[1]->{rect}->{width}, 155, 'bottom-right window got 155 px width');
cmp_float($nodes->[1]->{nodes}->[1]->{rect}->{height}, 135, 'bottom-right window got 135 px height');

# Without specifying mode
cmd 'resize set 201 131';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[1]->{nodes}->[1]->{rect}->{width}, 201, 'bottom-right window got 201 px width');
cmp_float($nodes->[1]->{nodes}->[1]->{rect}->{height}, 131, 'bottom-right window got 131 px height');

# Mix ppt and px
cmd 'resize set 75 ppt 200 px';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.25, 'left container got 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'right container got 75%');
cmp_float($nodes->[1]->{nodes}->[1]->{rect}->{height}, 200, 'bottom-right window got 200 px height');

############################################################
# resize from inside a tabbed container
############################################################

$tmp = fresh_workspace;

cmd 'split h';

$left = open_window;
my $right1 = open_window;

cmd 'split h';
cmd 'layout tabbed';

my $right2 = open_window;

diag("left = " . $left->id . ", right1 = " . $right1->id . ", right2 = " . $right2->id);

is($x->input_focus, $right2->id, '2nd right window focused');

cmd 'resize set 75 ppt 0 ppt';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.25, 'left container got 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'right container got 75%');

# Same but with px.
cmd 'resize set 155 px';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[1]->{rect}->{width}, 155, 'right container got 155 px');

############################################################
# resize from inside a stacked container
############################################################

$tmp = fresh_workspace;

cmd 'split h';

$left = open_window;
$right1 = open_window;

cmd 'split h';
cmd 'layout stacked';

$right2 = open_window;

diag("left = " . $left->id . ", right1 = " . $right1->id . ", right2 = " . $right2->id);

is($x->input_focus, $right2->id, '2nd right window focused');

cmd 'resize set 75 ppt 0 ppt';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.25, 'left container got 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'right container got 75%');

# Same but with px.
cmd 'resize set 130 px';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[1]->{rect}->{width}, 130, 'right container got 130 px');

done_testing;
