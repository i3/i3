#!perl
# vim:ts=4:sw=4:expandtab
# When using a command which moves a window to scratchpad from an invisible
# (e.g. unfocused) workspace and immediately shows that window again, i3
# crashed.
# Bug still in: 4.2-305-g22922a9
use i3test;

my $ws1 = fresh_workspace;
my $invisible_window = open_window;
my $other_focusable_window = open_window;

my $ws2 = fresh_workspace;
my $id = $invisible_window->id;
cmd qq|[id="$id"] move scratchpad, scratchpad show|;

does_i3_live;

done_testing;
