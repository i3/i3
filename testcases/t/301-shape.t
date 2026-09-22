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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
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

my $ws; # set by run_test
my ($bg_win, $bg_win_focus, $shaped_win, $shaped_win_focus); # set by the subtests

subtest 'normal border: make floating window, then set shape', \&run_test => sub {
    $bg_win = open_floating_window(rect => [50, 50, 200, 200], background_color => '#ff0000');
    $bg_win_focus = get_focused($ws);

    $shaped_win = open_floating_window(rect => [100, 100, 100, 100], background_color => '#00ff00');
    cmd '[id=' . $shaped_win->id . '] border normal 10px';
    $shaped_win_focus = get_focused($ws);
    set_shape($shaped_win->id);
};

subtest 'normal border: set shape first, then make window floating', \&run_test, => sub {
    $bg_win = open_window(rect => [50, 50, 200, 200], background_color => '#ff0000');
    $bg_win_focus = get_focused($ws);
    cmd 'floating toggle';

    $shaped_win = open_window(rect => [100, 100, 100, 100], background_color => '#00ff00');
    cmd '[id=' . $shaped_win->id . '] border normal 10px';
    $shaped_win_focus = get_focused($ws);
    set_shape($shaped_win->id);
    cmd 'floating toggle';
};

subtest 'pixel border: make floating window, then set shape', \&run_test => sub {
    $bg_win = open_floating_window(rect => [50, 50, 200, 200], background_color => '#ff0000');
    $bg_win_focus = get_focused($ws);

    $shaped_win = open_floating_window(rect => [100, 100, 100, 100], background_color => '#00ff00');
    cmd '[id=' . $shaped_win->id . '] border pixel 10px';
    $shaped_win_focus = get_focused($ws);
    set_shape($shaped_win->id);
};

subtest 'pixel border: set shape first, then make window floating', \&run_test, => sub {
    $bg_win = open_window(rect => [50, 50, 200, 200], background_color => '#ff0000');
    $bg_win_focus = get_focused($ws);
    cmd 'floating toggle';

    $shaped_win = open_window(rect => [100, 100, 100, 100], background_color => '#00ff00');
    cmd '[id=' . $shaped_win->id . '] border pixel 10px';
    $shaped_win_focus = get_focused($ws);
    set_shape($shaped_win->id);
    cmd 'floating toggle';
};

done_testing;

sub run_test {
    my ($setup_sub) = @_;
    $ws = fresh_workspace;
    $setup_sub->();

    # Test the input region by observing the focus_follows_mouse behavior.
    # The visual conunterpart (clip region) is not tested, but it uses the same
    # code path.

    # 4    5    6
    #  ┌───┬───┐
    #  │ 1 │ 2 │
    # 7├───┴───┤8
    #  │   3   │
    #  └───────┘
    #      9
    my @points_table = (
        { x =>  25, y =>  25, opaque => 1, name => '1: window zone A' },
        { x =>  75, y =>  25, opaque => 0, name => '2: window zone B' },
        { x =>  50, y =>  75, opaque => 0, name => '3: window zone C' },

        { x =>  -5, y =>  -5, opaque => 1, name => '4: titlebar left' },
        { x =>  50, y =>  -5, opaque => 1, name => '5: titlebar center' },
        { x => 105, y =>  -5, opaque => 1, name => '6: titlebar right' },

        { x =>  50, y => 105, opaque => 1, name => '7: border left' },
        { x =>  -5, y =>  50, opaque => 1, name => '8: border right' },
        { x => 105, y =>  50, opaque => 1, name => '9: border bottom' },
    );

    for my $p (@points_table) {
        $shaped_win->warp_pointer($p->{x}, $p->{y});
        sync_with_i3;
        if ($p->{opaque}) {
            is(get_focused($ws), $shaped_win_focus, "opaque      - $p->{name}");
        } else {
            is(get_focused($ws), $bg_win_focus,     "passthrough - $p->{name}");
        }
    }

    kill_all_windows;
}
