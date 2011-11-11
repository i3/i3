#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests if WM_STATE is WM_STATE_NORMAL when mapped and WM_STATE_WITHDRAWN when
# unmapped.
#
use X11::XCB qw(:all);
use i3test;

my $x = X11::XCB::Connection->new;

my $window = open_window($x);

sync_with_i3($x);

is($window->state, ICCCM_WM_STATE_NORMAL, 'WM_STATE normal');

$window->unmap;

wait_for_unmap $x;

is($window->state, ICCCM_WM_STATE_WITHDRAWN, 'WM_STATE withdrawn');

done_testing;
