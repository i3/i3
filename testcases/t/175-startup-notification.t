#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Test for the startup notification protocol.
#

use i3test;
use POSIX qw(mkfifo);
use File::Temp qw(:POSIX);

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
static xcb_connection_t *conn;

// TODO: this should use $x
void init_ctx() {
    int screen;
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
    xcb_flush(conn);
}

void complete_startup() {
    /* mark the startup process complete */
    sn_launchee_context_complete(ctx);
}
END_OF_C_CODE

my $first_ws = fresh_workspace;

is_num_children($first_ws, 0, 'no containers on this workspace yet');

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

isnt($startup_id, '', 'startup_id not empty');

$ENV{DESKTOP_STARTUP_ID} = $startup_id;

# Create a new libstartup-notification launchee context
init_ctx();

# Make sure the context was set up successfully
is(get_startup_id(), $startup_id, 'libstartup-notification returns the same id');

my $second_ws = fresh_workspace;

is_num_children($second_ws, 0, 'no containers on the second workspace yet');

my $win = open_window({ dont_map => 1 });
mark_window($win->id);
$win->map;
# We don’t use wait_for_map because the window will not get mapped -- it is on
# a different workspace.
# We sync with i3 here to make sure $x->input_focus is updated.
sync_with_i3;

is_num_children($second_ws, 0, 'still no containers on the second workspace');
is_num_children($first_ws, 1, 'one container on the first workspace');

######################################################################
# same thing, but with _NET_STARTUP_ID set on the leader
######################################################################

my $leader = open_window({ dont_map => 1 });
mark_window($leader->id);

$win = open_window({ dont_map => 1, client_leader => $leader });
$win->map;
sync_with_i3;

is_num_children($second_ws, 0, 'still no containers on the second workspace');
is_num_children($first_ws, 2, 'two containers on the first workspace');

######################################################################
# 2) open another window after the startup process is completed
# (should be placed on the current workspace)
######################################################################

complete_startup();
sync_with_i3;

my $otherwin = open_window;
is_num_children($second_ws, 1, 'one container on the second workspace');

######################################################################
# 3) test that the --no-startup-id flag for exec leads to no DESKTOP_STARTUP_ID
# environment variable.
######################################################################

mkfifo($tmp, 0600) or die "Could not create FIFO in $tmp";

cmd qq|exec --no-startup-id echo \$DESKTOP_STARTUP_ID >$tmp|;

open($fh, '<', $tmp);
chomp($startup_id = <$fh>);
close($fh);

unlink($tmp);

is($startup_id, '', 'startup_id empty');

######################################################################
# 4) same thing, but with double quotes in exec
######################################################################

mkfifo($tmp, 0600) or die "Could not create FIFO in $tmp";

cmd qq|exec --no-startup-id "echo \$DESKTOP_STARTUP_ID >$tmp"|;

open($fh, '<', $tmp);
chomp($startup_id = <$fh>);
close($fh);

unlink($tmp);

is($startup_id, '', 'startup_id empty');

done_testing;
