#!perl
# vim:ts=4:sw=4:expandtab
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
$content = get_ws_content($tmp);
is(@{$content}, 3, 'three nodes on this workspace');

# due to automatic flattening/cleanup, the remaining split container
# will be replaced by the con itself, so we will still have 3 nodes
cmd 'move right';
$content = get_ws_content($tmp);
is(@{$content}, 2, 'two nodes on this workspace');

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

$content = get_ws_content($otmp);
is(@{$content}, 1, 'only one nodes on this workspace');

######################################################################
# 5) test moving floating containers.
######################################################################

$tmp = fresh_workspace;
my $floatwin = open_floating_window($x);
my ($absolute_before, $top_before) = $floatwin->rect;

cmd 'move left';

my ($absolute, $top) = $floatwin->rect;

is($absolute->x, ($absolute_before->x - 10), 'moved 10 px to the left');
is($absolute->y, $absolute_before->y, 'y not changed');
is($absolute->width, $absolute_before->width, 'width not changed');
is($absolute->height, $absolute_before->height, 'height not changed');

$absolute_before = $absolute;
$top_before = $top;

cmd 'move right';

($absolute, $top) = $floatwin->rect;

is($absolute->x, ($absolute_before->x + 10), 'moved 10 px to the right');
is($absolute->y, $absolute_before->y, 'y not changed');
is($absolute->width, $absolute_before->width, 'width not changed');
is($absolute->height, $absolute_before->height, 'height not changed');

$absolute_before = $absolute;
$top_before = $top;

cmd 'move up';

($absolute, $top) = $floatwin->rect;

is($absolute->x, $absolute_before->x, 'x not changed');
is($absolute->y, ($absolute_before->y - 10), 'moved 10 px up');
is($absolute->width, $absolute_before->width, 'width not changed');
is($absolute->height, $absolute_before->height, 'height not changed');

$absolute_before = $absolute;
$top_before = $top;

cmd 'move down';

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

($absolute, $top) = $floatwin->rect;

is($absolute->x, ($absolute_before->x - 20), 'moved 10 px to the left');
is($absolute->y, $absolute_before->y, 'y not changed');
is($absolute->width, $absolute_before->width, 'width not changed');
is($absolute->height, $absolute_before->height, 'height not changed');



done_testing;
