#!perl
# vim:ts=4:sw=4:expandtab
#
# Checks if the 'move [window/container] to workspace' command works correctly
#
use i3test;

my $i3 = i3(get_socket_path());

# We move the pointer out of our way to avoid a bug where the focus will
# be set to the window under the cursor
$x->root->warp_pointer(0, 0);

sub move_workspace_test {
    my ($movecmd) = @_;

    my $tmp = get_unused_workspace();
    my $tmp2 = get_unused_workspace();
    cmd "workspace $tmp";

    ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

    my $first = open_empty_con($i3);
    my $second = open_empty_con($i3);
    ok(@{get_ws_content($tmp)} == 2, 'two containers on first ws');

    cmd "workspace $tmp2";
    ok(@{get_ws_content($tmp2)} == 0, 'no containers on second ws yet');

    cmd "workspace $tmp";

    cmd "$movecmd $tmp2";
    ok(@{get_ws_content($tmp)} == 1, 'one container on first ws anymore');
    ok(@{get_ws_content($tmp2)} == 1, 'one container on second ws');
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
ok(@{get_ws_content('13: meh')} == 1, 'one container on 13: meh');

ok(!workspace_exists('13'), 'workspace 13 does not exist yet');

cmd 'workspace 12';
cmd 'open';

cmd 'move to workspace number 13';
ok(@{get_ws_content('13: meh')} == 2, 'two containers on 13: meh');
ok(@{get_ws_content('12')} == 0, 'no container on 12 anymore');

ok(!workspace_exists('13'), 'workspace 13 does still not exist');

###################################################################
# check if 'move workspace next' and 'move workspace prev' work
###################################################################

# Open two containers on the first workspace, one container on the second
# workspace. Because the workspaces are named, they will be sorted by order of
# creation.
my $tmp = get_unused_workspace();
my $tmp2 = get_unused_workspace();
cmd "workspace $tmp";
ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
my $first = open_empty_con($i3);
my $second = open_empty_con($i3);
ok(@{get_ws_content($tmp)} == 2, 'two containers on first ws');

cmd "workspace $tmp2";
ok(@{get_ws_content($tmp2)} == 0, 'no containers yet');
my $third = open_empty_con($i3);
ok(@{get_ws_content($tmp2)} == 1, 'one container on second ws');

# go back to the first workspace, move one of the containers to the next one
cmd "workspace $tmp";
cmd 'move workspace next';
ok(@{get_ws_content($tmp)} == 1, 'one container on first ws');
ok(@{get_ws_content($tmp2)} == 2, 'two containers on second ws');

# go to the second workspace and move two containers to the first one
cmd "workspace $tmp2";
cmd 'move workspace prev';
cmd 'move workspace prev';
ok(@{get_ws_content($tmp)} == 3, 'three containers on first ws');
ok(@{get_ws_content($tmp2)} == 0, 'no containers on second ws');

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

done_testing;
