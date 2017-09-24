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
    set_xkb_group
    xtest_key_press
    xtest_key_release
    xtest_button_press
    xtest_button_release
    listen_for_binding
    start_binding_capture
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
    %sn_config = ExtUtils::PkgConfig->find('xcb-xkb xcb-xtest');
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

static xcb_connection_t *conn = NULL;

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

    return true;
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

my $i3;
our @binding_events;

=head2 start_binding_capture()

Captures all binding events sent by i3 in the C<@binding_events> symbol, so
that you can verify the correct number of binding events was generated.

  my $pid = launch_with_config($config);
  start_binding_capture;
  # …
  sync_with_i3;
  is(scalar @i3test::XTEST::binding_events, 2, 'Received exactly 2 binding events');

=cut

sub start_binding_capture {
    # Store a copy of each binding event so that we can count the expected
    # events in test cases.
    $i3 = i3(get_socket_path());
    $i3->connect()->recv;
    $i3->subscribe({
        binding => sub {
            my ($event) = @_;
            @binding_events = (@binding_events, $event);
        },
    })->recv;
}

=head2 listen_for_binding($cb)

Helper function to evaluate whether sending KeyPress/KeyRelease events via
XTEST triggers an i3 key binding or not (with a timeout of 0.5s). Expects key
bindings to be configured in the form “bindsym <binding> nop <binding>”, e.g.
“bindsym Mod4+Return nop Mod4+Return”.

  is(listen_for_binding(
      sub {
          xtest_key_press(133); # Super_L
          xtest_key_press(36); # Return
          xtest_key_release(36); # Return
          xtest_key_release(133); # Super_L
      },
      ),
     'Mod4+Return',
     'triggered the "Mod4+Return" keybinding');

=cut

sub listen_for_binding {
    my ($cb) = @_;
    my $triggered = AnyEvent->condvar;
    my $i3 = i3(get_socket_path());
    $i3->connect()->recv;
    $i3->subscribe({
        binding => sub {
            my ($event) = @_;
            return unless $event->{change} eq 'run';
            # We look at the command (which is “nop <binding>”) because that is
            # easier than re-assembling the string representation of
            # $event->{binding}.
            $triggered->send($event->{binding}->{command});
        },
    })->recv;

    my $t;
    $t = AnyEvent->timer(
        after => 0.5,
        cb => sub {
            $triggered->send('timeout');
        }
    );

    $cb->();

    my $recv = $triggered->recv;
    $recv =~ s/^nop //g;
    return $recv;
}

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

=head1 AUTHOR

Michael Stapelberg <michael@i3wm.org>

=cut

1
