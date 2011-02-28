#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression test for setting a window to floating, tiling and opening a new window
#
use Time::HiRes qw(sleep);
use i3test;

my $tmp = get_unused_workspace;
cmd "workspace $tmp";


cmd 'open';
cmd 'mode toggle';
cmd 'mode toggle';
cmd 'open';

does_i3_live;

done_testing;
