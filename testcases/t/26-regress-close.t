#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression: closing of floating clients did crash i3 when closing the
# container which contained this client.
#
use i3test;

fresh_workspace;

cmd 'open';
cmd 'mode toggle';
cmd 'kill';
cmd 'kill';

does_i3_live;

done_testing;
