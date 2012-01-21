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
sync_with_i3;

cmd 'fullscreen';
sync_with_i3;

cmd 'restart';

does_i3_live;

done_testing;
