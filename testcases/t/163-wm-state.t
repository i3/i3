#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests if WM_STATE is WM_STATE_NORMAL when mapped and WM_STATE_WITHDRAWN when
# unmapped.
#
use i3test;
use X11::XCB qw(ICCCM_WM_STATE_NORMAL ICCCM_WM_STATE_WITHDRAWN);

my $window = open_window;

is($window->state, ICCCM_WM_STATE_NORMAL, 'WM_STATE normal');

$window->unmap;

wait_for_unmap $window;

is($window->state, ICCCM_WM_STATE_WITHDRAWN, 'WM_STATE withdrawn');

done_testing;
