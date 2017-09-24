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
# Test that movement of a con into a branch will place the moving con at the
# correct position within the branch.
#
# If the direction of movement is the same as the orientation of the branch
# container, append or prepend the container to the branch in the obvious way.
# If the movement is to the right or downward, insert the moving container in
# the first position (i.e., the leftmost or top position resp.) If the movement
# is to the left or upward, insert the moving container in the last position
# (i.e., the rightmost or bottom position resp.)
#
# If the direction of movement is different from the orientation of the branch
# container, insert the container into the branch after the focused-inactive
# container.
#
# For testing purposes, we will demonstrate the behavior for tabbed containers
# to represent the case of split-horizontal branches and stacked containers to
# represent the case of split-vertical branches.
#
# Ticket: #1060
# Bug still in: 4.6-109-g18cfc36

use i3test;

# Opens tabs on the presently focused branch and adds several additional
# windows. Shifts focus to somewhere in the middle of the tabs so the most
# general case can be assumed.
sub open_tabs {
    cmd 'layout tabbed';
    open_window;
    open_window;
    open_window;
    open_window;
    cmd 'focus left; focus left'
}

# Likewise for a stack
sub open_stack {
    cmd 'layout stacking';
    open_window;
    open_window;
    open_window;
    open_window;
    cmd 'focus up; focus up'
}

# Gets the position of the given leaf within the given branch. The first
# position is one (1). Returns negative one (-1) if the leaf cannot be found
# within the branch.
sub get_leaf_position {
    my ($branch, $leaf) = @_;
    my $position = -1;
    for my $i (0 .. @{$branch->{nodes}}) {
        if ($branch->{nodes}[$i]->{id} == $leaf) {
            $position = $i + 1;
            last;
        };
    }
    return $position;
}

# convenience function to focus a con by id to avoid having to type an ugly
# command each time
sub focus_con {
    my $con_id = shift @_;
    cmd "[con_id=\"$con_id\"] focus";
}

# Places a leaf into a branch and focuses the leaf. The newly created branch
# will have orientation specified by the second parameter.
sub branchify {
    my ($con_id, $orientation) = @_;
    focus_con($con_id);
    $orientation eq 'horizontal' ? cmd 'splith' : cmd 'splitv';
    open_window;
    focus_con($con_id);
}

##############################################################################
# When moving a con right into tabs, the moving con should be placed as the
# first tab in the branch
##############################################################################
my $ws = fresh_workspace;

# create the target leaf
open_window;
my $target_leaf = get_focused($ws);

# create the tabbed branch container
open_window;
cmd 'splith';
open_tabs;

# move the target leaf into the tabbed branch
focus_con($target_leaf);
cmd 'move right';

# the target leaf should be the first in the branch
my $branch = shift @{get_ws_content($ws)};
is($branch->{nodes}[0]->{id}, $target_leaf, 'moving con right into tabs placed it as the first tab in the branch');

# repeat the test when the target is in a branch
cmd 'move up; move left';
branchify($target_leaf, 'vertical');
cmd 'move right';

$branch = pop @{get_ws_content($ws)};
is($branch->{nodes}[0]->{id}, $target_leaf, 'moving con right into tabs from a branch placed it as the first tab in the branch');

##############################################################################
# When moving a con right into a stack, the moving con should be placed
# below the focused-inactive leaf
##############################################################################
$ws = fresh_workspace;

# create the target leaf
open_window;
$target_leaf = get_focused($ws);

# create the stacked branch container and find the focused leaf
open_window;
cmd 'splith';
open_stack;
my $secondary_leaf = get_focused($ws);

# move the target leaf into the stacked branch
focus_con($target_leaf);
cmd 'move right';

# the secondary focus leaf should be below the target
$branch = shift @{get_ws_content($ws)};
my $target_leaf_position = get_leaf_position($branch, $target_leaf);
my $secondary_leaf_position = get_leaf_position($branch, $secondary_leaf);

is($target_leaf_position, $secondary_leaf_position + 1, 'moving con right into a stack placed it below the focused-inactive leaf');

# repeat the test when the target is in a branch
cmd 'move up; move left';
branchify($target_leaf, 'vertical');
cmd 'move right';

$branch = pop @{get_ws_content($ws)};
$target_leaf_position = get_leaf_position($branch, $target_leaf);
$secondary_leaf_position = get_leaf_position($branch, $secondary_leaf);

is($target_leaf_position, $secondary_leaf_position + 1, 'moving con right into a stack from a branch placed it below the focused-inactive leaf');

##############################################################################
# When moving a con down into a stack, the moving con should be placed at the
# top of the stack
##############################################################################
$ws = fresh_workspace;
cmd 'layout splitv';

# create the target leaf
open_window;
$target_leaf = get_focused($ws);

# create the stacked branch container
open_window;
cmd 'splitv';
open_stack;

# move the target leaf into the stacked branch
focus_con($target_leaf);
cmd 'move down';

# the target leaf should be on the top of the stack
$branch = shift @{get_ws_content($ws)};
is($branch->{nodes}[0]->{id}, $target_leaf, 'moving con down into a stack placed it on the top of the stack');

# repeat the test when the target is in a branch
cmd 'move right; move up';
branchify($target_leaf, 'horizontal');
cmd 'move down';

$branch = pop @{get_ws_content($ws)};
is($branch->{nodes}[0]->{id}, $target_leaf, 'moving con down into a stack from a branch placed it on the top of the stack');

##############################################################################
# When moving a con down into tabs, the moving con should be placed after the
# focused-inactive tab
##############################################################################
$ws = fresh_workspace;
cmd 'layout splitv';

# create the target leaf
open_window;
$target_leaf = get_focused($ws);

# create the tabbed branch container and find the focused tab
open_window;
cmd 'splitv';
open_tabs;
$secondary_leaf = get_focused($ws);

# move the target leaf into the tabbed branch
focus_con($target_leaf);
cmd 'move down';

# the secondary focus tab should be to the right
$branch = shift @{get_ws_content($ws)};
$target_leaf_position = get_leaf_position($branch, $target_leaf);
$secondary_leaf_position = get_leaf_position($branch, $secondary_leaf);

is($target_leaf_position, $secondary_leaf_position + 1, 'moving con down into tabs placed it after the focused-inactive tab');

# repeat the test when the target is in a branch
cmd 'move right; move up';
branchify($target_leaf, 'horizontal');
cmd 'move down';

$branch = pop @{get_ws_content($ws)};
$target_leaf_position = get_leaf_position($branch, $target_leaf);
$secondary_leaf_position = get_leaf_position($branch, $secondary_leaf);

is($target_leaf_position, $secondary_leaf_position + 1, 'moving con down into tabs from a branch placed it after the focused-inactive tab');

##############################################################################
# When moving a con left into tabs, the moving con should be placed as the last
# tab in the branch
##############################################################################
$ws = fresh_workspace;

# create the tabbed branch container
open_window;
cmd 'splith';
open_tabs;

# create the target leaf
cmd 'focus parent';
open_window;
$target_leaf = get_focused($ws);

# move the target leaf into the tabbed branch
cmd 'move left';

# the target leaf should be last in the branch
$branch = shift @{get_ws_content($ws)};

is($branch->{nodes}->[-1]->{id}, $target_leaf, 'moving con left into tabs placed it as the last tab in the branch');

# repeat the test when the target leaf is in a branch
cmd 'move up; move right';
branchify($target_leaf, 'vertical');
cmd 'move left';

$branch = shift @{get_ws_content($ws)};
is($branch->{nodes}->[-1]->{id}, $target_leaf, 'moving con left into tabs from a branch placed it as the last tab in the branch');

##############################################################################
# When moving a con left into a stack, the moving con should be placed below
# the focused-inactive leaf
##############################################################################
$ws = fresh_workspace;

# create the stacked branch container and find the focused leaf
open_window;
open_stack;
$secondary_leaf = get_focused($ws);

# create the target leaf to the right
cmd 'focus parent';
open_window;
$target_leaf = get_focused($ws);

# move the target leaf into the stacked branch
cmd 'move left';

# the secondary focus leaf should be below
$branch = shift @{get_ws_content($ws)};
$target_leaf_position = get_leaf_position($branch, $target_leaf);
$secondary_leaf_position = get_leaf_position($branch, $secondary_leaf);

is($target_leaf_position, $secondary_leaf_position + 1, 'moving con left into a stack placed it below the focused-inactive leaf');

# repeat the test when the target leaf is in a branch
cmd 'move up; move right';
branchify($target_leaf, 'vertical');
cmd 'move left';

$branch = shift @{get_ws_content($ws)};
$target_leaf_position = get_leaf_position($branch, $target_leaf);
$secondary_leaf_position = get_leaf_position($branch, $secondary_leaf);

is($target_leaf_position, $secondary_leaf_position + 1, 'moving con left into a stack from a branch placed it below the focused-inactive leaf');

##############################################################################
# When moving a con up into a stack, the moving con should be placed last in
# the stack
##############################################################################
$ws = fresh_workspace;
cmd 'layout splitv';

# create the stacked branch container
open_window;
cmd 'splitv';
open_stack;

# create the target leaf
cmd 'focus parent';
open_window;
$target_leaf = get_focused($ws);

# move the target leaf into the stacked branch
cmd 'move up';

# the target leaf should be on the bottom of the stack
$branch = shift @{get_ws_content($ws)};

is($branch->{nodes}->[-1]->{id}, $target_leaf, 'moving con up into stack placed it on the bottom of the stack');

# repeat the test when the target leaf is in a branch
cmd 'move right; move down';
branchify($target_leaf, 'horizontal');
cmd 'move up';

$branch = shift @{get_ws_content($ws)};

is($branch->{nodes}->[-1]->{id}, $target_leaf, 'moving con up into stack from a branch placed it on the bottom of the stack');

##############################################################################
# When moving a con up into tabs, the moving con should be placed after the
# focused-inactive tab
##############################################################################
$ws = fresh_workspace;
cmd 'layout splitv';

# create the tabbed branch container and find the focused leaf
open_window;
cmd 'splitv';
open_tabs;
$secondary_leaf = get_focused($ws);

# create the target leaf below
cmd 'focus parent';
open_window;
$target_leaf = get_focused($ws);

# move the target leaf into the tabbed branch
cmd 'move up';

# the secondary focus tab should be to the right
$branch = shift @{get_ws_content($ws)};
$target_leaf_position = get_leaf_position($branch, $target_leaf);
$secondary_leaf_position = get_leaf_position($branch, $secondary_leaf);

is($target_leaf_position, $secondary_leaf_position + 1, 'moving con up into tabs placed it after the focused-inactive tab');

# repeat the test when the target leaf is in a branch
cmd 'move right; move down';
branchify($target_leaf, 'horizontal');
cmd 'move up';

$branch = shift @{get_ws_content($ws)};
$target_leaf_position = get_leaf_position($branch, $target_leaf);
$secondary_leaf_position = get_leaf_position($branch, $secondary_leaf);

is($target_leaf_position, $secondary_leaf_position + 1, 'moving con up into tabs from a branch placed it after the focused-inactive tab');

done_testing;
