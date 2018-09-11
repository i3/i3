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
# Checks if the 'move [window/container] to workspace' command works correctly
#
use i3test;

my $i3 = i3(get_socket_path());

# We move the pointer out of our way to avoid a bug where the focus will
# be set to the window under the cursor
sync_with_i3;
$x->root->warp_pointer(0, 0);
sync_with_i3;

sub move_workspace_test {
    my ($movecmd) = @_;

    my $tmp = get_unused_workspace();
    my $tmp2 = get_unused_workspace();
    cmd "workspace $tmp";

    is_num_children($tmp, 0, 'no containers yet');

    my $first = open_empty_con($i3);
    my $second = open_empty_con($i3);
    is_num_children($tmp, 2, 'two containers on first ws');

    cmd "workspace $tmp2";
    is_num_children($tmp2, 0, 'no containers on second ws yet');

    cmd "workspace $tmp";

    cmd "$movecmd $tmp2";
    is_num_children($tmp, 1, 'one container on first ws anymore');
    is_num_children($tmp2, 1, 'one container on second ws');
    my ($nodes, $focus) = get_ws_content($tmp2);

    is($focus->[0], $second, 'same container on different ws');

    ($nodes, $focus) = get_ws_content($tmp);
    ok($nodes->[0]->{focused}, 'first container focused on first ws');
}

move_workspace_test('move workspace');  # supported for legacy reasons
move_workspace_test('move to workspace');
# Those are just synonyms and more verbose ways of saying the same thing:
move_workspace_test('move window to workspace');
move_workspace_test('move container to workspace');

################################################################################
# Check that 'move to workspace number <number>' works to move a window to
# named workspaces which start with <number>.
################################################################################

cmd 'workspace 13: meh';
cmd 'open';
is_num_children('13: meh', 1, 'one container on 13: meh');

ok(!workspace_exists('13'), 'workspace 13 does not exist yet');

cmd 'workspace 12';
cmd 'open';

cmd 'move to workspace number 13';
is_num_children('13: meh', 2, 'one container on 13: meh');
is_num_children('12', 0, 'no container on 12 anymore');

ok(!workspace_exists('13'), 'workspace 13 does still not exist');

################################################################################
# Check that 'move to workspace number <number><name>' works to move a window to
# named workspaces which start with <number>.
################################################################################

cmd 'workspace 15: meh';
cmd 'open';
is_num_children('15: meh', 1, 'one container on 15: meh');

ok(!workspace_exists('15'), 'workspace 15 does not exist yet');
ok(!workspace_exists('15: duh'), 'workspace 15: duh does not exist yet');

cmd 'workspace 14';
cmd 'open';

cmd 'move to workspace number 15: duh';
is_num_children('15: meh', 2, 'two containers on 15: meh');
is_num_children('14', 0, 'no container on 14 anymore');

ok(!workspace_exists('15'), 'workspace 15 does still not exist');
ok(!workspace_exists('15: duh'), 'workspace 15 does still not exist');

###################################################################
# check if 'move workspace next' and 'move workspace prev' work
###################################################################

# Open two containers on the first workspace, one container on the second
# workspace. Because the workspaces are named, they will be sorted by order of
# creation.
my $tmp = get_unused_workspace();
my $tmp2 = get_unused_workspace();
cmd "workspace $tmp";
is_num_children($tmp, 0, 'no containers yet');
my $first = open_empty_con($i3);
my $second = open_empty_con($i3);
is_num_children($tmp, 2, 'two containers');

cmd "workspace $tmp2";
is_num_children($tmp2, 0, 'no containers yet');
my $third = open_empty_con($i3);
is_num_children($tmp2, 1, 'one container on second ws');

# go back to the first workspace, move one of the containers to the next one
cmd "workspace $tmp";
cmd 'move workspace next';
is_num_children($tmp, 1, 'one container on first ws');
is_num_children($tmp2, 2, 'two containers on second ws');

# go to the second workspace and move two containers to the first one
cmd "workspace $tmp2";
cmd 'move workspace prev';
cmd 'move workspace prev';
is_num_children($tmp, 3, 'three containers on first ws');
is_num_children($tmp2, 0, 'no containers on second ws');

###################################################################
# check if 'move workspace current' works
###################################################################

$tmp = get_unused_workspace();
$tmp2 = get_unused_workspace();

cmd "workspace $tmp";
$first = open_window(name => 'win-name');
is_num_children($tmp, 1, 'one container on first ws');

cmd "workspace $tmp2";
is_num_children($tmp2, 0, 'no containers yet');

cmd qq|[title="win-name"] move workspace $tmp2|;
is_num_children($tmp2, 1, 'one container on second ws');

cmd qq|[title="win-name"] move workspace $tmp|;
is_num_children($tmp2, 0, 'no containers on second ws');

###################################################################
# check if floating cons are moved to new workspaces properly
# (that is, if they are floating on the target ws, too)
###################################################################

$tmp = get_unused_workspace();
$tmp2 = get_unused_workspace();
cmd "workspace $tmp";

cmd "open";
cmd "floating toggle";

my $ws = get_ws($tmp);
is(@{$ws->{nodes}}, 0, 'no nodes on workspace');
is(@{$ws->{floating_nodes}}, 1, 'one floating node on workspace');

cmd "move workspace $tmp2";

$ws = get_ws($tmp2);
is(@{$ws->{nodes}}, 0, 'no nodes on workspace');
is(@{$ws->{floating_nodes}}, 1, 'one floating node on workspace');

################################################################################
# Check that 'move workspace number' works correctly.
################################################################################

$tmp = get_unused_workspace();
cmd 'open';

cmd 'workspace 16';
cmd 'open';
is_num_children('16', 1, 'one node on ws 16');

cmd "workspace $tmp";
cmd 'open';
cmd 'move workspace number 16';
is_num_children('16', 2, 'two nodes on ws 16');

ok(!workspace_exists('17'), 'workspace 17 does not exist yet');
cmd 'open';
cmd 'move workspace number 17';
ok(workspace_exists('17'), 'workspace 17 created by moving');
is(@{get_ws('17')->{nodes}}, 1, 'one node on ws 16');

################################################################################
# The following four tests verify the various 'move workspace' commands when
# the selection is itself a workspace.
################################################################################

# borrowed from 122-split.t
# recursively sums up all nodes and their children
sub sum_nodes {
    my ($nodes) = @_;

    return 0 if !@{$nodes};

    my @children = (map { @{$_->{nodes}} } @{$nodes},
                    map { @{$_->{'floating_nodes'}} } @{$nodes});

    return @{$nodes} + sum_nodes(\@children);
}

############################################################
# move workspace 'next|prev'
############################################################
$tmp = get_unused_workspace();
$tmp2 = get_unused_workspace();

cmd "workspace $tmp";
cmd 'open';
is_num_children($tmp, 1, 'one container on first ws');

cmd "workspace $tmp2";
cmd 'open';
is_num_children($tmp2, 1, 'one container on second ws');
cmd 'open';
is_num_children($tmp2, 2, 'two containers on second ws');

cmd 'focus parent';
cmd 'move workspace prev';

is_num_children($tmp, 2, 'two child containers on first ws');
is(sum_nodes(get_ws_content($tmp)), 4, 'four total containers on first ws');
is_num_children($tmp2, 0, 'no containers on second ws');

############################################################
# move workspace current
# This is a special case that should be a no-op.
############################################################
$tmp = fresh_workspace();

cmd 'open';
is_num_children($tmp, 1, 'one container on first ws');
my $tmpcount = sum_nodes(get_ws_content($tmp));

cmd 'focus parent';
cmd "move workspace $tmp";

is(sum_nodes(get_ws_content($tmp)), $tmpcount, 'number of containers in first ws unchanged');

############################################################
# move workspace '<name>'
############################################################
$tmp2 = get_unused_workspace();
$tmp = fresh_workspace();

cmd 'open';
is_num_children($tmp, 1, 'one container on first ws');

cmd "workspace $tmp2";
cmd 'open';
is_num_children($tmp2, 1, 'one container on second ws');
cmd 'open';
is_num_children($tmp2, 2, 'two containers on second ws');

cmd 'focus parent';
cmd "move workspace $tmp";

is_num_children($tmp, 2, 'two child containers on first ws');
is(sum_nodes(get_ws_content($tmp)), 4, 'four total containers on first ws');
is_num_children($tmp2, 0, 'no containers on second ws');

############################################################
# move workspace number '<number>'
############################################################
cmd 'workspace 18';
cmd 'open';
is_num_children('18', 1, 'one container on ws 18');

cmd 'workspace 19';
cmd 'open';
is_num_children('19', 1, 'one container on ws 19');
cmd 'open';
is_num_children('19', 2, 'two containers on ws 19');

cmd 'focus parent';
cmd 'move workspace number 18';

is_num_children('18', 2, 'two child containers on ws 18');
is(sum_nodes(get_ws_content('18')), 4, 'four total containers on ws 18');
is_num_children('19', 0, 'no containers on ws 19');

###################################################################
# move workspace '<name>' with a floating child
###################################################################
$tmp2 = get_unused_workspace();
$tmp = fresh_workspace();
cmd 'open';
cmd 'floating toggle';
cmd 'open';
cmd 'floating toggle';
cmd 'open';

$ws = get_ws($tmp);
is_num_children($tmp, 1, 'one container on first workspace');
is(@{$ws->{floating_nodes}}, 2, 'two floating nodes on first workspace');

cmd 'focus parent';
cmd "move workspace $tmp2";

$ws = get_ws($tmp2);
is_num_children($tmp2, 1, 'one container on second workspace');
is(@{$ws->{floating_nodes}}, 2, 'two floating nodes on second workspace');

###################################################################
# same as the above, but with only floating children
###################################################################
$tmp2 = get_unused_workspace();
$tmp = fresh_workspace();
cmd 'open';
cmd 'floating toggle';

$ws = get_ws($tmp);
is_num_children($tmp, 0, 'no regular nodes on first workspace');
is(@{$ws->{floating_nodes}}, 1, 'one floating node on first workspace');

cmd 'focus parent';
cmd "move workspace $tmp2";

$ws = get_ws($tmp2);
is_num_children($tmp2, 0, 'no regular nodes on second workspace');
is(@{$ws->{floating_nodes}}, 1, 'one floating node on second workspace');

###################################################################
# Check that moving an empty workspace using criteria doesn't
# create unfocused empty workspace.
###################################################################
$tmp2 = get_unused_workspace();
$tmp = fresh_workspace();
cmd 'mark a';
cmd "[con_mark=a] move to workspace $tmp2";

is (get_ws($tmp2), undef, 'No empty workspace created');

done_testing;
