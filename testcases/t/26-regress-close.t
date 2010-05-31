#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression: closing of floating clients did crash i3 when closing the
# container which contained this client.
#
use i3test tests => 1;
use X11::XCB qw(:all);
use v5.10;

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

$i3->command('open')->recv;
$i3->command('mode toggle')->recv;
$i3->command('kill')->recv;
$i3->command('kill')->recv;


my $tree = $i3->get_workspaces->recv;
my @nodes = @{$tree->{nodes}};
ok(@nodes > 0, 'i3 still lives');

diag( "Testing i3, Perl $], $^X" );
