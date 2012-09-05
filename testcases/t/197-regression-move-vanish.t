#!perl
# vim:ts=4:sw=4:expandtab
# Regression test: moving a window to the right out of a splitv container would
# make it vanish.
# Ticket: #790
# Bug still in: 4.2-277-ga598544
use i3test;

my $ws = fresh_workspace;

my $top = open_window;
cmd 'split v';
my $bottom = open_window;

is_num_children($ws, 2, 'two windows on workspace level');

cmd 'move right';

is_num_children($ws, 2, 'still two windows on workspace level');

done_testing;
