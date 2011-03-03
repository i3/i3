#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression test for using level-up to get to the 'content'-container and
# toggle floating
#
use Time::HiRes qw(sleep);
use i3test;

my $tmp = get_unused_workspace;
cmd "workspace $tmp";


cmd 'open';
cmd 'level up';
cmd 'level up';
cmd 'mode toggle';

does_i3_live;

done_testing;
