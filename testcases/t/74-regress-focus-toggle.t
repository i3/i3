#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression: Checks if i3 still lives after using 'focus mode_toggle' on an
# empty workspace. This regression was fixed in
# 0848844f2d41055f6ffc69af1149d7a873460976.
#
use i3test;
use v5.10;

my $tmp = fresh_workspace;

cmd 'focus mode_toggle';

does_i3_live;

done_testing;
