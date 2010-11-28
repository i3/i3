#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests moving. Basically, there are four different code-paths:
# 1) move a container which cannot be moved (single container on a workspace)
# 2) move a container before another single container
# 3) move a container inside another container
# 4) move a container in a different direction so that we need to go up in tree
#
use i3test tests => 16;
use X11::XCB qw(:all);

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

######################################################################
# 1) move a container which cannot be moved
######################################################################

$i3->command('open')->recv;

my $old_content = get_ws_content($tmp);
is(@{$old_content}, 1, 'one container on this workspace');

my $first = $old_content->[0]->{id};

#$i3->command('move before h')->recv;
#$i3->command('move before v')->recv;
#$i3->command('move after v')->recv;
#$i3->command('move after h')->recv;

my $content = get_ws_content($tmp);
#is_deeply($old_content, $content, 'workspace unmodified after useless moves');

######################################################################
# 2) move a container before another single container
######################################################################

$i3->command('open')->recv;
$content = get_ws_content($tmp);
is(@{$content}, 2, 'two containers on this workspace');
my $second = $content->[1]->{id};

is($content->[0]->{id}, $first, 'first container unmodified');

# Move the second container before the first one (→ swap them)
$i3->command('move before h')->recv;
$content = get_ws_content($tmp);
is($content->[0]->{id}, $second, 'first container modified');

# We should not be able to move any further
$i3->command('move before h')->recv;
$content = get_ws_content($tmp);
is($content->[0]->{id}, $second, 'first container unmodified');

# Now move in the other direction
$i3->command('move after h')->recv;
$content = get_ws_content($tmp);
is($content->[0]->{id}, $first, 'first container modified');

# We should not be able to move any further
$i3->command('move after h')->recv;
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
$i3->command('split v')->recv;
$i3->command('level up')->recv;
$i3->command('open')->recv;

$content = get_ws_content($tmp);
is(@{$content}, 3, 'three containers on this workspace');
my $third = $content->[2]->{id};

$i3->command('move before h')->recv;
$content = get_ws_content($tmp);
is(@{$content}, 2, 'only two containers on this workspace');
my $nodes = $content->[1]->{nodes};
is($nodes->[0]->{id}, $second, 'second container on top');
is($nodes->[1]->{id}, $third, 'third container on bottom');

######################################################################
# move it inside the split container
######################################################################

$i3->command('move before v')->recv;
$nodes = get_ws_content($tmp)->[1]->{nodes};
is($nodes->[0]->{id}, $third, 'third container on top');
is($nodes->[1]->{id}, $second, 'second container on bottom');

# move it outside again
$i3->command('move before h')->recv;
$content = get_ws_content($tmp);
is(@{$content}, 3, 'three nodes on this workspace');

$i3->command('move after h')->recv;
$content = get_ws_content($tmp);
is(@{$content}, 2, 'two nodes on this workspace');

######################################################################
# 4) We create two v-split containers on the workspace, then we move
#    all Cons from the left v-split to the right one. The old vsplit
#    container needs to be closed. Verify that it will be closed.
######################################################################

my $otmp = get_unused_workspace();
$i3->command("workspace $otmp")->recv;

$i3->command("open")->recv;
$i3->command("open")->recv;
$i3->command("split v")->recv;
$i3->command("open")->recv;
$i3->command("prev h")->recv;
$i3->command("split v")->recv;
$i3->command("open")->recv;
$i3->command("move after h")->recv;
$i3->command("prev h")->recv;
$i3->command("move after h")->recv;

$content = get_ws_content($otmp);
is(@{$content}, 1, 'only one nodes on this workspace');

diag( "Testing i3, Perl $], $^X" );
