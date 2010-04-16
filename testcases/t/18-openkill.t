#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests whether opening an empty container and killing it again works
#
use Test::More tests => 3;
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

sub get_unused_workspace {
    my @names = get_workspace_names();
    my $tmp;
    do { $tmp = tmpnam() } while ($tmp ~~ @names);
    $tmp
}

sub get_ws_content {
    my ($name) = @_;
    my $tree = $i3->get_workspaces->recv;
    my @ws = map { @{$_->{nodes}} } @{$tree->{nodes}};
    my @cons = map { $_->{nodes} } grep { $_->{name} eq $name } @ws;
    return $cons[0];
}

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

# Open a new container
$i3->command("open")->recv;

ok(@{get_ws_content($tmp)} == 1, 'container opened');

$i3->command("kill")->recv;
ok(@{get_ws_content($tmp)} == 0, 'container killed');

diag( "Testing i3, Perl $], $^X" );
