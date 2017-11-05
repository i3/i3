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

############################################################
# resize vertically
############################################################

my $tmp = fresh_workspace;

cmd 'split v';

my $top = open_window;
my $bottom = open_window;

diag("top = " . $top->id . ", bottom = " . $bottom->id);

is($x->input_focus, $bottom->id, 'Bottom window focused');

cmd 'resize set 0 ppt 75 ppt';

my ($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.25, 'top window got only 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'bottom window got 75%');


############################################################
# resize horizontally and vertically
############################################################

my $tmp = fresh_workspace;

cmd 'split h';
my $left = open_window;
my $top_right = open_window;
cmd 'split v';
my $bottom_right = open_window;

diag("left = " . $left->id . ", top-right = " . $top_right->id . ", bottom-right = " . $bottom_right->id);

is($x->input_focus, $bottom_right->id, 'Bottom-right window focused');

cmd 'resize set 75 ppt 75 ppt';

my ($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.25, 'left container got 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'right container got 75%');
cmp_float($nodes->[1]->{nodes}->[0]->{percent}, 0.25, 'top-right window got 25%');
cmp_float($nodes->[1]->{nodes}->[1]->{percent}, 0.75, 'bottom-right window got 75%');


############################################################
# resize from inside a tabbed container
############################################################

my $tmp = fresh_workspace;

cmd 'split h';

my $left = open_window;
my $right1 = open_window;

cmd 'split h';
cmd 'layout tabbed';

my $right2 = open_window;

diag("left = " . $left->id . ", right1 = " . $right1->id . ", right2 = " . $right2->id);

is($x->input_focus, $right2->id, '2nd right window focused');

cmd 'resize set 75 ppt 0 ppt';

my ($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.25, 'left container got 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'right container got 75%');


############################################################
# resize from inside a stacked container
############################################################

my $tmp = fresh_workspace;

cmd 'split h';

my $left = open_window;
my $right1 = open_window;

cmd 'split h';
cmd 'layout stacked';

my $right2 = open_window;

diag("left = " . $left->id . ", right1 = " . $right1->id . ", right2 = " . $right2->id);

is($x->input_focus, $right2->id, '2nd right window focused');

cmd 'resize set 75 ppt 0 ppt';

my ($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.25, 'left container got 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'right container got 75%');


done_testing;
