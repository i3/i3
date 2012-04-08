#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests whether we can switch to a non-existant workspace
# (necessary for further tests)
#
use List::Util qw(first);
use i3test;

# to ensure that workspace 1 stays open
cmd 'open';

my $tmp = fresh_workspace;
ok(workspace_exists($tmp), 'workspace created');
# if the workspace could not be created, we cannot run any other test
# (every test starts by creating its workspace)
if (!workspace_exists($tmp)) {
    BAIL_OUT('Cannot create workspace, further tests make no sense');
}

my $otmp = fresh_workspace;
diag("Other temporary workspace name: $otmp\n");

# As the old workspace was empty, it should get
# cleaned up as we switch away from it
cmd "workspace $otmp";
ok(!workspace_exists($tmp), 'old workspace cleaned up');

# Switch to the same workspace again to make sure it doesn’t get cleaned up
cmd "workspace $otmp";
cmd "workspace $otmp";
ok(workspace_exists($otmp), 'other workspace still exists');


#####################################################################
# check if the workspace next / prev commands work
#####################################################################

cmd 'workspace next';

ok(!workspace_exists('next'), 'workspace "next" does not exist');

cmd "workspace $tmp";
cmd 'open';

ok(workspace_exists($tmp), 'workspace created');

cmd "workspace $otmp";
cmd 'open';

ok(workspace_exists($tmp), 'workspace tmp still exists');
ok(workspace_exists($otmp), 'workspace otmp created');

is(focused_ws(), $otmp, 'focused workspace is otmp');

cmd 'workspace prev';
is(focused_ws(), $tmp, 'focused workspace is tmp after workspace prev');

cmd 'workspace next';
is(focused_ws(), $otmp, 'focused workspace is otmp after workspace next');


#####################################################################
# check that wrapping works
#####################################################################

cmd 'workspace next';
is(focused_ws(), '1', 'focused workspace is 1 after workspace next');

cmd 'workspace next';
is(focused_ws(), $tmp, 'focused workspace is tmp after workspace next');

cmd 'workspace next';
is(focused_ws(), $otmp, 'focused workspace is otmp after workspace next');


cmd 'workspace prev';
is(focused_ws(), $tmp, 'focused workspace is tmp after workspace prev');

cmd 'workspace prev';
is(focused_ws(), '1', 'focused workspace is tmp after workspace prev');

cmd 'workspace prev';
is(focused_ws(), $otmp, 'focused workspace is otmp after workspace prev');


#####################################################################
# check if we can change to "next" / "prev"
#####################################################################

cmd 'workspace "next"';

ok(workspace_exists('next'), 'workspace "next" exists');
is(focused_ws(), 'next', 'now on workspace next');

cmd 'workspace "prev"';

ok(workspace_exists('prev'), 'workspace "prev" exists');
is(focused_ws(), 'prev', 'now on workspace prev');

#####################################################################
# check that the numbers are assigned/recognized correctly
#####################################################################

cmd "workspace 3: $tmp";
my $ws = get_ws("3: $tmp");
ok(defined($ws), "workspace 3: $tmp was created");
is($ws->{num}, 3, 'workspace number is 3');

cmd "workspace 0: $tmp";
$ws = get_ws("0: $tmp");
ok(defined($ws), "workspace 0: $tmp was created");
is($ws->{num}, 0, 'workspace number is 0');

cmd "workspace aa: $tmp";
$ws = get_ws("aa: $tmp");
ok(defined($ws), "workspace aa: $tmp was created");
is($ws->{num}, -1, 'workspace number is -1');

################################################################################
# Check that we can go to workspace "4: foo" with the command
# "workspace number 4".
################################################################################

ok(!workspace_exists('4'), 'workspace 4 does not exist');
ok(!workspace_exists('4: foo'), 'workspace 4: foo does not exist yet');
cmd 'workspace 4: foo';
ok(workspace_exists('4: foo'), 'workspace 4: foo was created');
cmd 'open';

cmd 'workspace 3';
ok(workspace_exists('4: foo'), 'workspace 4: foo still open');
cmd 'workspace number 4';
is(focused_ws(), '4: foo', 'now on workspace 4: foo');
ok(!workspace_exists('4'), 'workspace 4 still does not exist');

################################################################################
# Verify that renaming workspaces works.
################################################################################

sub workspace_numbers_sorted {
    my ($name) = @_;
    my $i3 = i3(get_socket_path());
    my $tree = $i3->get_tree->recv;

    my @outputs = @{$tree->{nodes}};
    my @workspaces;
    for my $output (@outputs) {
        # get the first CT_CON of each output
        my $content = first { $_->{type} == 2 } @{$output->{nodes}};
        @workspaces = (@workspaces, @{$content->{nodes}});
    }

    my @numbers = grep { $_ != -1 } map { $_->{num} } @workspaces;
    is_deeply(
        [ sort { $a <=> $b } @numbers ],
        \@numbers,
        'workspace numbers sorted');
}

# 1: numbered workspace
cmd 'workspace 10';
cmd 'open';
cmd 'workspace 13';
cmd 'open';

workspace_numbers_sorted();

cmd 'workspace 9';
is(focused_ws(), '9', 'now on workspace 9');

ok(!workspace_exists('12'), 'workspace 12 does not exist yet');
cmd 'rename workspace 9 to 12';
ok(!workspace_exists('9'), 'workspace 9 does not exist anymore');
is(focused_ws(), '12', 'now on workspace 12');
$ws = get_ws('12');
is($ws->{num}, 12, 'number correctly changed');

workspace_numbers_sorted();

# 2: numbered + named workspace
cmd 'workspace 9: foo';
is(focused_ws(), '9: foo', 'now on workspace 9: foo');

ok(!workspace_exists('11: bar'), 'workspace 11: bar does not exist yet');
cmd 'rename workspace "9: foo" to "11: bar"';
ok(!workspace_exists('9: foo'), 'workspace 9 does not exist anymore');
is(focused_ws(), '11: bar', 'now on workspace 10');
$ws = get_ws('11: bar');
is($ws->{num}, 11, 'number correctly changed');
workspace_numbers_sorted();
# keep that one open, we need it later
cmd 'open';

# 3: named workspace
cmd 'workspace bleh';
is(focused_ws(), 'bleh', 'now on workspace bleh');

ok(!workspace_exists('qux'), 'workspace qux does not exist yet');
cmd 'rename workspace bleh to qux';
ok(!workspace_exists('bleh'), 'workspace 9 does not exist anymore');
is(focused_ws(), 'qux', 'now on workspace qux');
$ws = get_ws('qux');
is($ws->{num}, -1, 'number correctly changed');
workspace_numbers_sorted();

# 5: already existing workspace
my $result = cmd 'rename workspace qux to 11: bar';
ok(!$result->[0]->{success}, 'renaming workspace to an already existing one failed');

# 6: non-existing old workspace (verify command result)
$result = cmd 'rename workspace notexistant to bleh';
ok(!$result->[0]->{success}, 'renaming workspace which does not exist failed');


done_testing;
