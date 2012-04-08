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

done_testing;
