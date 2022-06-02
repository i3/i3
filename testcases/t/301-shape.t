#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • https://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • https://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • https://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Test shape support.
# Ticket: #2742
use i3test;
use ExtUtils::PkgConfig;

my %sn_config;
BEGIN {
    %sn_config = ExtUtils::PkgConfig->find('xcb-shape');
}

use Inline C => Config => LIBS => $sn_config{libs}, CCFLAGS => $sn_config{cflags};
use Inline C => <<'END_OF_C_CODE';
#include <xcb/shape.h>

static xcb_connection_t *conn;

void init_ctx(void *connptr) {
    conn = (xcb_connection_t*)connptr;
}

/*
 * Set the shape for the window consisting of the following zones:
 *
 *   +---+---+
 *   | A | B |
 *   +---+---+
 *   |   C   |
 *   +-------+
 *
 * - Zone A is completely opaque.
 * - Zone B is clickable through (input shape).
 * - Zone C is completely transparent (bounding shape).
 */
void set_shape(long window_id) {
    xcb_rectangle_t bounding_rectangle = { 0, 0, 100, 50 };
    xcb_shape_rectangles(conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING,
                         XCB_CLIP_ORDERING_UNSORTED, window_id,
                         0, 0, 1, &bounding_rectangle);
    xcb_rectangle_t input_rectangle = { 0, 0, 50, 50 };
    xcb_shape_rectangles(conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT,
                         XCB_CLIP_ORDERING_UNSORTED, window_id,
                         0, 0, 1, &input_rectangle);
    xcb_flush(conn);
}
END_OF_C_CODE

init_ctx($x->get_xcb_conn());

my ($ws, $win1, $win1_focus, $win2, $win2_focus);

################################################################################
# Case 1: make floating window, then set shape
################################################################################

$ws = fresh_workspace;

$win1 = open_floating_window(rect => [0, 0, 100, 100], background_color => '#ff0000');
$win1_focus = get_focused($ws);

$win2 = open_floating_window(rect => [0, 0, 100, 100], background_color => '#00ff00');
$win2_focus = get_focused($ws);
set_shape($win2->id);

$win1->warp_pointer(75, 25);
sync_with_i3;
is(get_focused($ws), $win1_focus, 'focus switched to the underlying window');

$win1->warp_pointer(25, 25);
sync_with_i3;
is(get_focused($ws), $win2_focus, 'focus switched to the top window');

kill_all_windows;

################################################################################
# Case 2: set shape first, then make window floating
################################################################################

$ws = fresh_workspace;

$win1 = open_window(rect => [0, 0, 100, 100], background_color => '#ff0000');
$win1_focus = get_focused($ws);
cmd 'floating toggle';

$win2 = open_window(rect => [0, 0, 100, 100], background_color => '#00ff00');
$win2_focus = get_focused($ws);
set_shape($win2->id);
cmd 'floating toggle';
sync_with_i3;

$win1->warp_pointer(75, 25);
sync_with_i3;
is(get_focused($ws), $win1_focus, 'focus switched to the underlying window');

$win1->warp_pointer(25, 25);
sync_with_i3;
is(get_focused($ws), $win2_focus, 'focus switched to the top window');

done_testing;
