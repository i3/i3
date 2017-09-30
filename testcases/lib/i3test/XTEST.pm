package i3test::XTEST;
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use v5.10;

use Test::More;
use i3test::Util qw(get_socket_path);
use lib qw(@abs_top_srcdir@/AnyEvent-I3/blib/lib);
use AnyEvent::I3;
use ExtUtils::PkgConfig;

use Exporter ();
our @EXPORT = qw(
    inlinec_connect
    xtest_sync_with
    xtest_sync_with_i3
    set_xkb_group
    xtest_key_press
    xtest_key_release
    xtest_button_press
    xtest_button_release
    binding_events
);

=encoding utf-8

=head1 NAME

i3test::XTEST - Inline::C wrappers for xcb-xtest and xcb-xkb

=cut

# We need to use libxcb-xkb because xdotool cannot trigger ISO_Next_Group
# anymore: it contains code to set the XKB group to 1 and then restore the
# previous group, effectively rendering any keys that switch groups
# ineffective.
my %sn_config;
BEGIN {
    %sn_config = ExtUtils::PkgConfig->find('xcb-xkb xcb-xtest xcb-util');
}

use Inline C => Config => LIBS => $sn_config{libs}, CCFLAGS => $sn_config{cflags};
use Inline C => <<'END_OF_C_CODE';
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include <xcb/xcb.h>
#include <xcb/xkb.h>
#include <xcb/xtest.h>
#include <xcb/xcb_aux.h>

static xcb_connection_t *conn = NULL;
static xcb_window_t sync_window;
static xcb_window_t root_window;
static xcb_atom_t i3_sync_atom;

bool inlinec_connect() {
    int screen;

    if ((conn = xcb_connect(NULL, &screen)) == NULL ||
        xcb_connection_has_error(conn)) {
        if (conn != NULL) {
            xcb_disconnect(conn);
        }
        fprintf(stderr, "Could not connect to X11\n");
        return false;
    }

    if (!xcb_get_extension_data(conn, &xcb_xkb_id)->present) {
        fprintf(stderr, "XKB not present\n");
        return false;
    }

    if (!xcb_get_extension_data(conn, &xcb_test_id)->present) {
        fprintf(stderr, "XTEST not present\n");
        return false;
    }

    xcb_generic_error_t *err = NULL;
    xcb_xkb_use_extension_reply_t *usereply;
    usereply = xcb_xkb_use_extension_reply(
        conn, xcb_xkb_use_extension(conn, XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION), &err);
    if (err != NULL || usereply == NULL) {
        fprintf(stderr, "xcb_xkb_use_extension() failed\n");
        free(err);
        return false;
    }
    free(usereply);

    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, xcb_intern_atom(conn, 0, strlen("I3_SYNC"), "I3_SYNC"), NULL);
    i3_sync_atom = reply->atom;
    free(reply);

    xcb_screen_t *root_screen = xcb_aux_get_screen(conn, screen);
    root_window = root_screen->root;
    sync_window = xcb_generate_id(conn);
    xcb_create_window(conn,
                      XCB_COPY_FROM_PARENT,           // depth
                      sync_window,                    // window
                      root_window,                    // parent
                      -15,                            // x
                      -15,                            // y
                      1,                              // width
                      1,                              // height
                      0,                              // border_width
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,  // class
                      XCB_COPY_FROM_PARENT,           // visual
                      XCB_CW_OVERRIDE_REDIRECT,       // value_mask
                      (uint32_t[]){
                          1,  // override_redirect
                      });     // value_list

    return true;
}

void xtest_sync_with(int window) {
    xcb_client_message_event_t ev;
    memset(&ev, '\0', sizeof(xcb_client_message_event_t));

    const int nonce = rand() % 255;

    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = sync_window;
    ev.type = i3_sync_atom;
    ev.format = 32;
    ev.data.data32[0] = sync_window;
    ev.data.data32[1] = nonce;

    xcb_send_event(conn, false, (xcb_window_t)window, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (char *)&ev);
    xcb_flush(conn);

    xcb_generic_event_t *event = NULL;
    while (1) {
        free(event);
        if ((event = xcb_wait_for_event(conn)) == NULL) {
            break;
        }
        if (event->response_type == 0) {
            fprintf(stderr, "X11 Error received! sequence %x\n", event->sequence);
            continue;
        }

        /* Strip off the highest bit (set if the event is generated) */
        const int type = (event->response_type & 0x7F);
        switch (type) {
            case XCB_CLIENT_MESSAGE: {
                xcb_client_message_event_t *ev = (xcb_client_message_event_t *)event;
                {
                    const uint32_t got = ev->data.data32[0];
                    const uint32_t want = sync_window;
                    if (got != want) {
                        fprintf(stderr, "Ignoring ClientMessage: unknown window: got %d, want %d\n", got, want);
                        continue;
                    }
                }
                {
                    const uint32_t got = ev->data.data32[1];
                    const uint32_t want = nonce;
                    if (got != want) {
                        fprintf(stderr, "Ignoring ClientMessage: unknown nonce: got %d, want %d\n", got, want);
                        continue;
                    }
                }
                return;
            }
            default:
                fprintf(stderr, "Unexpected X11 event of type %d received (XCB_CLIENT_MESSAGE = %d)\n", type, XCB_CLIENT_MESSAGE);
                break;
        }
    }
    free(event);
}

void xtest_sync_with_i3() {
    xtest_sync_with((int)root_window);
}

// NOTE: while |group| should be a uint8_t, Inline::C will not define the
// function unless we use an int.
bool set_xkb_group(int group) {
    xcb_generic_error_t *err = NULL;
    // Needs libxcb ≥ 1.11 so that we have the following bug fix:
    // https://cgit.freedesktop.org/xcb/proto/commit/src/xkb.xml?id=8d7ee5b6ba4cf343f7df70372a3e1f85b82aeed7
    xcb_void_cookie_t cookie = xcb_xkb_latch_lock_state_checked(
        conn,
        XCB_XKB_ID_USE_CORE_KBD, /* deviceSpec */
        0,                       /* affectModLocks */
        0,                       /* modLocks */
        1,                       /* lockGroup */
        group,                   /* groupLock */
        0,                       /* affectModLatches */
        0,                       /* latchGroup */
        0);                      /* groupLatch */
    if ((err = xcb_request_check(conn, cookie)) != NULL) {
        fprintf(stderr, "X error code %d\n", err->error_code);
        free(err);
        return false;
    }
    return true;
}

bool xtest_input(int type, int detail, int x, int y) {
    xcb_generic_error_t *err;
    xcb_void_cookie_t cookie;

    cookie = xcb_test_fake_input_checked(
        conn,
        type,             /* type */
        detail,           /* detail */
        XCB_CURRENT_TIME, /* time */
        XCB_NONE,         /* root */
        x,                /* rootX */
        y,                /* rootY */
        XCB_NONE);        /* deviceid */
    if ((err = xcb_request_check(conn, cookie)) != NULL) {
        fprintf(stderr, "X error code %d\n", err->error_code);
        free(err);
        return false;
    }

    return true;
}

bool xtest_key(int type, int detail) {
    return xtest_input(type, detail, 0, 0);
}

bool xtest_key_press(int detail) {
    return xtest_key(XCB_KEY_PRESS, detail);
}

bool xtest_key_release(int detail) {
    return xtest_key(XCB_KEY_RELEASE, detail);
}

bool xtest_button_press(int button, int x, int y) {
    return xtest_input(XCB_BUTTON_PRESS, button, x, y);
}

bool xtest_button_release(int button, int x, int y) {
    return xtest_input(XCB_BUTTON_RELEASE, button, x, y);
}

END_OF_C_CODE

sub import {
    my ($class, %args) = @_;
    ok(inlinec_connect(), 'Connect to X11, verify XKB and XTEST are present (via Inline::C)');
    goto \&Exporter::import;
}

=head1 EXPORT

=cut

=head2 set_xkb_group($group)

Changes the current XKB group from the default of 1 to C<$group>, which must be
one of 1, 2, 3, 4.

Returns false when there was an X11 error changing the group, true otherwise.

=head2 xtest_key_press($detail)

Sends a KeyPress event via XTEST, with the specified C<$detail>, i.e. key code.
Use C<xev(1)> to find key codes.

Returns false when there was an X11 error, true otherwise.

=head2 xtest_key_release($detail)

Sends a KeyRelease event via XTEST, with the specified C<$detail>, i.e. key code.
Use C<xev(1)> to find key codes.

Returns false when there was an X11 error, true otherwise.

=head2 xtest_button_press($button, $x, $y)

Sends a ButtonPress event via XTEST, with the specified C<$button>.

Returns false when there was an X11 error, true otherwise.

=head2 xtest_button_release($button, $x, $y)

Sends a ButtonRelease event via XTEST, with the specified C<$button>.

Returns false when there was an X11 error, true otherwise.

=head2 xtest_sync_with($window)

Ensures the specified window has processed all X11 events which were triggered
by this module, provided the window response to the i3 sync protocol.

=head2 xtest_sync_with_i3()

Ensures i3 has processed all X11 events which were triggered by this module.

=head1 AUTHOR

Michael Stapelberg <michael@i3wm.org>

=cut

1
