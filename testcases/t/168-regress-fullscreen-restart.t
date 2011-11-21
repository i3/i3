#!perl
# vim:ts=4:sw=4:expandtab
#
# Verifies that i3 survives inplace restarts with fullscreen containers
#
use i3test;

fresh_workspace;

open_window;
open_window;

cmd 'layout stacking';
sleep 1;

cmd 'fullscreen';
sleep 1;

cmd 'restart';
sleep 1;

does_i3_live;

done_testing;
