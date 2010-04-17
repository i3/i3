#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests whether we can switch to a non-existant workspace
# (necessary for further tests)
#
use i3test tests => 2;
use v5.10;

my $i3 = i3("/tmp/nestedcons");

sub workspace_exists {
    my ($name) = @_;
    ($name ~~ @{get_workspace_names()})
}

my $tmp = get_unused_workspace();
diag("Temporary workspace name: $tmp\n");

$i3->command("workspace $tmp")->recv;
ok(workspace_exists($tmp), 'workspace created');
# if the workspace could not be created, we cannot run any other test
# (every test starts by creating its workspace)
if (!workspace_exists($tmp)) {
    BAIL_OUT('Cannot create workspace, further tests make no sense');
}

my $otmp = get_unused_workspace();
diag("Other temporary workspace name: $otmp\n");

# As the old workspace was empty, it should get
# cleaned up as we switch away from it
$i3->command("workspace $otmp")->recv;
ok(!workspace_exists($tmp), 'old workspace cleaned up');

diag( "Testing i3, Perl $], $^X" );
