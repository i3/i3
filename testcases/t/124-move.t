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
# Tests moving. Basically, there are four different code-paths:
# 1) move a container which cannot be moved (single container on a workspace)
# 2) move a container before another single container
# 3) move a container inside another container
# 4) move a container in a different direction so that we need to go up in tree
#
use i3test;

my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

######################################################################
# 1) move a container which cannot be moved
######################################################################

cmd 'open';

my $old_content = get_ws_content($tmp);
is(@{$old_content}, 1, 'one container on this workspace');

my $first = $old_content->[0]->{id};

cmd 'move left';
cmd 'move right';
cmd 'move up';
cmd 'move down';

my $content = get_ws_content($tmp);
is_deeply($old_content, $content, 'workspace unmodified after useless moves');

######################################################################
# 2) move a container before another single container
######################################################################

cmd 'open';
$content = get_ws_content($tmp);
is(@{$content}, 2, 'two containers on this workspace');
my $second = $content->[1]->{id};

is($content->[0]->{id}, $first, 'first container unmodified');

# Move the second container before the first one (→ swap them)
cmd 'move left';
$content = get_ws_content($tmp);
is($content->[0]->{id}, $second, 'first container modified');

# We should not be able to move any further
cmd 'move left';
$content = get_ws_content($tmp);
is($content->[0]->{id}, $second, 'first container unmodified');

# Now move in the other direction
cmd 'move right';
$content = get_ws_content($tmp);
is($content->[0]->{id}, $first, 'first container modified');

# We should not be able to move any further
cmd 'move right';
$content = get_ws_content($tmp);
is($content->[0]->{id}, $first, 'first container unmodified');

######################################################################
# 3) move a container inside another container
######################################################################

# Split the current (second) container and create a new container on workspace
# level. Our layout looks like this now:
# --------------------------
# |       | second |       |
# | first | ------ | third |
# |       |        |       |
# --------------------------
cmd 'split v';
cmd 'focus parent';
cmd 'open';

$content = get_ws_content($tmp);
is(@{$content}, 3, 'three containers on this workspace');
my $third = $content->[2]->{id};

cmd 'move left';
$content = get_ws_content($tmp);
is(@{$content}, 2, 'only two containers on this workspace');
my $nodes = $content->[1]->{nodes};
is($nodes->[0]->{id}, $second, 'second container on top');
is($nodes->[1]->{id}, $third, 'third container on bottom');

######################################################################
# move it inside the split container
######################################################################

cmd 'move up';
$nodes = get_ws_content($tmp)->[1]->{nodes};
is($nodes->[0]->{id}, $third, 'third container on top');
is($nodes->[1]->{id}, $second, 'second container on bottom');

# move it outside again
cmd 'move left';
is_num_children($tmp, 3, 'three containers after moving left');

# due to automatic flattening/cleanup, the remaining split container
# will be replaced by the con itself, so we will still have 3 nodes
cmd 'move right';
is_num_children($tmp, 2, 'two containers after moving right (flattening)');

######################################################################
# 4) We create two v-split containers on the workspace, then we move
#    all Cons from the left v-split to the right one. The old vsplit
#    container needs to be closed. Verify that it will be closed.
######################################################################

my $otmp = fresh_workspace;

cmd "open";
cmd "open";
cmd "split v";
cmd "open";
cmd 'focus left';
cmd "split v";
cmd "open";
cmd "move right";
cmd 'focus left';
cmd "move right";

is_num_children($otmp, 1, 'only one node on this workspace');

######################################################################
# 5) test moving floating containers.
######################################################################

$tmp = fresh_workspace;
my $floatwin = open_floating_window;
my ($absolute_before, $top_before) = $floatwin->rect;

cmd 'move left';

sync_with_i3;

my ($absolute, $top) = $floatwin->rect;

is($absolute->x, ($absolute_before->x - 10), 'moved 10 px to the left');
is($absolute->y, $absolute_before->y, 'y not changed');
is($absolute->width, $absolute_before->width, 'width not changed');
is($absolute->height, $absolute_before->height, 'height not changed');

$absolute_before = $absolute;
$top_before = $top;

cmd 'move right';

sync_with_i3;

($absolute, $top) = $floatwin->rect;

is($absolute->x, ($absolute_before->x + 10), 'moved 10 px to the right');
is($absolute->y, $absolute_before->y, 'y not changed');
is($absolute->width, $absolute_before->width, 'width not changed');
is($absolute->height, $absolute_before->height, 'height not changed');

$absolute_before = $absolute;
$top_before = $top;

cmd 'move up';

sync_with_i3;

($absolute, $top) = $floatwin->rect;

is($absolute->x, $absolute_before->x, 'x not changed');
is($absolute->y, ($absolute_before->y - 10), 'moved 10 px up');
is($absolute->width, $absolute_before->width, 'width not changed');
is($absolute->height, $absolute_before->height, 'height not changed');

$absolute_before = $absolute;
$top_before = $top;

cmd 'move down';

sync_with_i3;

($absolute, $top) = $floatwin->rect;

is($absolute->x, $absolute_before->x, 'x not changed');
is($absolute->y, ($absolute_before->y + 10), 'moved 10 px up');
is($absolute->width, $absolute_before->width, 'width not changed');
is($absolute->height, $absolute_before->height, 'height not changed');

$absolute_before = $absolute;
$top_before = $top;

######################################################################
# 6) test moving floating containers with a specific amount of px
######################################################################

cmd 'move left 20 px';

sync_with_i3;

($absolute, $top) = $floatwin->rect;

is($absolute->x, ($absolute_before->x - 20), 'moved 20 px to the left');
is($absolute->y, $absolute_before->y, 'y not changed');
is($absolute->width, $absolute_before->width, 'width not changed');
is($absolute->height, $absolute_before->height, 'height not changed');

######################################################################
# 6) test moving floating window to a specified position
#    and to absolute center
######################################################################

$tmp = fresh_workspace;
open_floating_window; my @floatcon;

cmd 'move position 5 px 15 px';

@floatcon = @{get_ws($tmp)->{floating_nodes}};

is($floatcon[0]->{rect}->{x}, 5, 'moved to position 5 x');
is($floatcon[0]->{rect}->{y}, 15, 'moved to position 15 y');

cmd 'move absolute position center';

@floatcon = @{get_ws($tmp)->{floating_nodes}};

my $center_x = int($x->root->rect->width/2) - int($floatcon[0]->{rect}->{width}/2);
my $center_y = int($x->root->rect->height/2) - int($floatcon[0]->{rect}->{height}/2);

is($floatcon[0]->{rect}->{x}, $center_x, "moved to center at position $center_x x");
is($floatcon[0]->{rect}->{y}, $center_y, "moved to center at position $center_y y");

# Make sure the command works with criteria
open_floating_window;

@floatcon = @{get_ws($tmp)->{floating_nodes}};

cmd '[con_id="' . $floatcon[0]->{nodes}[0]->{id} . '"] move position 25 px 30 px';
cmd '[con_id="' . $floatcon[1]->{nodes}[0]->{id} . '"] move position 35 px 40 px';

@floatcon = @{get_ws($tmp)->{floating_nodes}};

is($floatcon[0]->{rect}->{x}, 25, 'moved to position 25 x with criteria');
is($floatcon[0]->{rect}->{y}, 30, 'moved to position 30 y with criteria');

is($floatcon[1]->{rect}->{x}, 35, 'moved to position 35 x with criteria');
is($floatcon[1]->{rect}->{y}, 40, 'moved to position 40 y with criteria');

done_testing;
