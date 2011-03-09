#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests whether we can switch to a non-existant workspace
# (necessary for further tests)
#
use i3test;

sub workspace_exists {
    my ($name) = @_;
    ($name ~~ @{get_workspace_names()})
}

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

done_testing;
