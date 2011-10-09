#!perl
# vim:ts=4:sw=4:expandtab
#
# Test for the startup notification protocol.
#

use i3test;
use POSIX qw(mkfifo);
use File::Temp qw(:POSIX);

my $x = X11::XCB::Connection->new;
use ExtUtils::PkgConfig;

# setup dependency on libstartup-notification using pkg-config
my %sn_config;
BEGIN {
    %sn_config = ExtUtils::PkgConfig->find('libstartup-notification-1.0');
}

use Inline C => Config => LIBS => $sn_config{libs}, CCFLAGS => $sn_config{cflags};
use Inline C => <<'END_OF_C_CODE';

#include <xcb/xcb.h>

#define SN_API_NOT_YET_FROZEN 1
#include <libsn/sn-common.h>
#include <libsn/sn-launchee.h>

static SnDisplay *sndisplay;
static SnLauncheeContext *ctx;

// TODO: this should use $x
void init_ctx() {
    int screen;
    xcb_connection_t *conn;
    if ((conn = xcb_connect(NULL, &screen)) == NULL ||
        xcb_connection_has_error(conn))
        errx(1, "x11 conn failed");

    printf("screen = %d\n", screen);
    sndisplay = sn_xcb_display_new(conn, NULL, NULL);
    ctx = sn_launchee_context_new_from_environment(sndisplay, screen);
}

const char *get_startup_id() {
    return sn_launchee_context_get_startup_id(ctx);
}

void mark_window(int window) {
    sn_launchee_context_setup_window(ctx, (Window)window);
}

void complete_startup() {
    /* mark the startup process complete */
    sn_launchee_context_complete(ctx);
}
END_OF_C_CODE

my $first_ws = fresh_workspace;

is(@{get_ws_content($first_ws)}, 0, 'no containers on this workspace yet');

######################################################################
# 1) initiate startup, switch workspace, create window
# (should be placed on the original workspace)
######################################################################

# Start a new process via i3 (to initialize a new startup notification
# context), then steal its DESKTOP_STARTUP_ID variable. We handle the startup
# notification in the testcase from there on.
#
# This works by setting up a FIFO in which the process (started by i3) will
# echo its $DESKTOP_STARTUP_ID. We (blockingly) read the variable into
# $startup_id in the testcase.
my $tmp = tmpnam();
mkfifo($tmp, 0600) or die "Could not create FIFO in $tmp";

cmd qq|exec echo \$DESKTOP_STARTUP_ID >$tmp|;

open(my $fh, '<', $tmp);
chomp(my $startup_id = <$fh>);
close($fh);

unlink($tmp);

$ENV{DESKTOP_STARTUP_ID} = $startup_id;

# Create a new libstartup-notification launchee context
init_ctx();

# Make sure the context was set up successfully
is(get_startup_id(), $startup_id, 'libstartup-notification returns the same id');

my $second_ws = fresh_workspace;

is(@{get_ws_content($second_ws)}, 0, 'no containers on the second workspace yet');

my $win = open_window($x);
mark_window($win->id);

is(@{get_ws_content($second_ws)}, 0, 'still no containers on the second workspace');
is(@{get_ws_content($first_ws)}, 1, 'one container on the first workspace');

# TODO: the same thing, but in a CLIENT_LEADER situation

######################################################################
# 2) open another window after the startup process is completed
# (should be placed on the current workspace)
######################################################################

complete_startup();
sync_with_i3($x);

my $otherwin = open_window($x);
is(@{get_ws_content($second_ws)}, 1, 'one container on the second workspace');

done_testing;
