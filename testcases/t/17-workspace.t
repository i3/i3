#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests whether we can switch to a non-existant workspace
# (necessary for further tests)
#
use Test::More tests => 2;
use List::MoreUtils qw(all none);
use Data::Dumper;
use AnyEvent::I3;
use File::Temp qw(tmpnam);
use v5.10;

my $i3 = i3("/tmp/nestedcons");

sub get_workspace_names {
    my $tree = $i3->get_workspaces->recv;
    my @workspaces = map { @{$_->{nodes}} } @{$tree->{nodes}};
    [ map { $_->{name} } @workspaces ]
}

sub workspace_exists {
    my ($name) = @_;
    ($name ~~ @{get_workspace_names()})
}

sub get_unused_workspace {
    my @names = get_workspace_names();
    my $tmp;
    do { $tmp = tmpnam() } while ($tmp ~~ @names);
    $tmp
}

my $tmp = get_unused_workspace();
diag("Temporary workspace name: $tmp\n");

$i3->command("workspace $tmp")->recv;
ok(workspace_exists($tmp), 'workspace created');

my $otmp = get_unused_workspace();
diag("Other temporary workspace name: $otmp\n");

# As the old workspace was empty, it should get
# cleaned up as we switch away from it
$i3->command("workspace $otmp")->recv;
ok(!workspace_exists($tmp), 'old workspace cleaned up');

diag( "Testing i3, Perl $], $^X" );
