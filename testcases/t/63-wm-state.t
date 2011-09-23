#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests if WM_STATE is WM_STATE_NORMAL when mapped and WM_STATE_WITHDRAWN when
# unmapped.
#
use X11::XCB qw(:all);
use i3test;

BEGIN {
    use_ok('X11::XCB::Window');
    use_ok('X11::XCB::Event::Generic');
    use_ok('X11::XCB::Event::MapNotify');
    use_ok('X11::XCB::Event::ClientMessage');
}

my $x = X11::XCB::Connection->new;

my $window = open_standard_window($x);

sync_with_i3($x);

diag('window mapped');

is($window->state, ICCCM_WM_STATE_NORMAL, 'WM_STATE normal');

$window->unmap;

# TODO: wait for unmapnotify
sync_with_i3($x);

is($window->state, ICCCM_WM_STATE_WITHDRAWN, 'WM_STATE withdrawn');

done_testing;
