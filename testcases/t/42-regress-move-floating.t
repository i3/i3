#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression: move a floating window to a different workspace crashes i3
#
use i3test;

my $tmp = fresh_workspace;
my $otmp = get_unused_workspace();

cmd 'open';
cmd 'mode toggle';
cmd "move workspace $otmp";

does_i3_live;

done_testing;
