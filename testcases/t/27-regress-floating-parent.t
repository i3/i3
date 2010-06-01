#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression: make a container floating, kill its parent, make it tiling again
#
use i3test tests => 3;
use X11::XCB qw(:all);
use v5.10;

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

$i3->command('open')->recv;
$i3->command('open')->recv;
my $old = get_focused($tmp);
$i3->command('split v')->recv;
$i3->command('open')->recv;
my $floating = get_focused($tmp);
diag("focused floating: " . get_focused($tmp));
$i3->command('mode toggle')->recv;
# TODO: eliminate this race conditition
sleep 1;
$i3->command(qq|[con_id="$old"] focus|)->recv;
is(get_focused($tmp), $old, 'old container focused');

$i3->command('kill')->recv;
$i3->command('kill')->recv;
$i3->command(qq|[con_id="$floating"] focus|)->recv;
is(get_focused($tmp), $floating, 'floating window focused');

sleep 1;
$i3->command('mode toggle')->recv;

my $tree = $i3->get_workspaces->recv;
my @nodes = @{$tree->{nodes}};
ok(@nodes > 0, 'i3 still lives');

diag( "Testing i3, Perl $], $^X" );
