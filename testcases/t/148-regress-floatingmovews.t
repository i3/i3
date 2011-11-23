#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression test for correct focus behaviour when moving a floating con to
# another workspace.
#
use i3test;

my $tmp = fresh_workspace;

# open a tiling window on the first workspace
open_window;
my $first = get_focused($tmp);

# on a different ws, open a floating window
my $otmp = fresh_workspace;
open_window;
my $float = get_focused($otmp);
cmd 'mode toggle';
sync_with_i3;

# move the floating con to first workspace
cmd "move workspace $tmp";
sync_with_i3;

# switch to the first ws and check focus
is(get_focused($tmp), $float, 'floating client correctly focused');

done_testing;
