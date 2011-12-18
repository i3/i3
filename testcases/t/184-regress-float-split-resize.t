#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression: resizing a floating split container leads to a crash.
# (Ticket #588, present until 4412ccbe5a4fad8a4cd594e6f10f937515a4d37c)
#
use i3test;

my $tmp = fresh_workspace;

my $first = open_window;
cmd 'split v';
my $second = open_window;

cmd 'focus parent';
cmd 'floating toggle';
cmd 'layout stacking';

cmd 'resize grow up 10 px or 10 ppt';

does_i3_live;

done_testing;
