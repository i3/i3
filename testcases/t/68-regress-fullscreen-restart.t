#!perl
# vim:ts=4:sw=4:expandtab
#
# Verifies that i3 survives inplace restarts with fullscreen containers
#
use i3test;
use X11::XCB qw(:all);
use X11::XCB::Connection;

my $x = X11::XCB::Connection->new;

fresh_workspace;

open_window($x);
open_window($x);

cmd 'layout stacking';
sleep 1;

cmd 'fullscreen';
sleep 1;

cmd 'restart';
sleep 1;

does_i3_live;

done_testing;
