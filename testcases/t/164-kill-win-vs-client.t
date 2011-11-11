#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests if WM_STATE is WM_STATE_NORMAL when mapped and WM_STATE_WITHDRAWN when
# unmapped.
#
use i3test;

my $x = X11::XCB::Connection->new;

sub two_windows {
    my $tmp = fresh_workspace;

    ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

    my $first = open_window($x);
    my $second = open_window($x);

    sync_with_i3 $x;

    is($x->input_focus, $second->id, 'second window focused');
    ok(@{get_ws_content($tmp)} == 2, 'two containers opened');

    return $tmp;
}

##############################################################
# 1: open two windows (in the same client), kill one and see if
# the other one is still there
##############################################################

my $tmp = two_windows;

cmd 'kill';

sleep 0.25;

ok(@{get_ws_content($tmp)} == 1, 'one container left after killing');

##############################################################
# 2: same test case as test 1, but with the explicit variant
# 'kill window'
##############################################################

my $tmp = two_windows;

cmd 'kill window';

sleep 0.25;

ok(@{get_ws_content($tmp)} == 1, 'one container left after killing');

##############################################################
# 3: open two windows (in the same client), use 'kill client'
# and check if both are gone
##############################################################

my $tmp = two_windows;

cmd 'kill client';

sleep 0.25;

ok(@{get_ws_content($tmp)} == 0, 'no containers left after killing');

done_testing;
