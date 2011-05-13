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

my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#00ff00',
    event_mask => [ 'structure_notify' ],
);

$window->name('Window 1');
$window->map;

diag('window mapped');

sleep 0.5;

is($window->state, ICCCM_WM_STATE_NORMAL, 'WM_STATE normal');

$window->unmap;

sleep 0.5;

is($window->state, ICCCM_WM_STATE_WITHDRAWN, 'WM_STATE withdrawn');

done_testing;
