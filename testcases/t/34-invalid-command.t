#!perl
# vim:ts=4:sw=4:expandtab
#
# 
#
use i3test tests => 1;

my $i3 = i3("/tmp/nestedcons");

$i3->command("blargh!")->recv;

my $tree = $i3->get_tree->recv;
my @nodes = @{$tree->{nodes}};
ok(@nodes > 0, 'i3 still lives');

diag( "Testing i3, Perl $], $^X" );
