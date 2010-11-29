#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression: move a floating window to a different workspace crashes i3
#
use i3test tests => 1;
use X11::XCB qw(:all);

my $tmp = get_unused_workspace();
my $otmp = get_unused_workspace();
cmd "workspace $tmp";

cmd 'open';
cmd 'mode toggle';
cmd "move workspace $otmp";

my $tree = i3('/tmp/nestedcons')->get_tree->recv;
my @nodes = @{$tree->{nodes}};
ok(@nodes > 0, 'i3 still lives');
