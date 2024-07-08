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
# Tests for _NET_WM_DESKTOP.
# Ticket: #2153
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bar {
    status_command i3status
}
EOT
use X11::XCB qw(:all);

###############################################################################

sub get_net_wm_desktop {
    sync_with_i3;

    my ($con) = @_;
    my $cookie = $x->get_property(
        0,
        $con->{id},
        $x->atom(name => '_NET_WM_DESKTOP')->id,
        $x->atom(name => 'CARDINAL')->id,
        0,
        1
    );

    my $reply = $x->get_property_reply($cookie->{sequence});
    return undef if $reply->{length} != 1;

    return unpack("L", $reply->{value});
}

sub send_net_wm_desktop {
    my ($con, $idx) = @_;
    my $msg = pack "CCSLLLLLL",
        X11::XCB::CLIENT_MESSAGE, 32, 0,
        $con->{id},
        $x->atom(name => '_NET_WM_DESKTOP')->id,
        $idx, 0, 0, 0, 0;

    $x->send_event(0, $x->get_root_window(), X11::XCB::EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);
    sync_with_i3;
}

sub open_window_with_net_wm_desktop {
    my $idx = shift;
    my $window = open_window(
        before_map => sub {
            my ($window) = @_;
            $x->change_property(
                PROP_MODE_REPLACE,
                $window->id,
                $x->atom(name => '_NET_WM_DESKTOP')->id,
                $x->atom(name => 'CARDINAL')->id,
                32, 1,
                pack('L', $idx),
            );
        },
	dont_map => 1,
    );

    # We don’t wait for MapNotify and instead sync with i3 so that we don’t need
    # to encounter the full timeout of 4s when opening a window on a non-visible
    # workspace.
    $window->map;
    sync_with_i3;

    return $window;
}

###############################################################################
# Upon managing a window which does not set _NET_WM_DESKTOP, the property is
# set on the window.
###############################################################################

cmd 'workspace 1';
my $con = open_window;

is(get_net_wm_desktop($con), 0, '_NET_WM_DESKTOP is set upon managing a window');

kill_all_windows;

###############################################################################
# Upon managing a window which sets _NET_WM_DESKTOP, the window is moved to
# the specified desktop.
###############################################################################

cmd 'workspace 0';
open_window;
cmd 'workspace 1';
open_window;
cmd 'workspace 2';
open_window;

$con = open_window_with_net_wm_desktop(1);

is(get_net_wm_desktop($con), 1, '_NET_WM_DESKTOP still has the correct value');
is_num_children('1', 2, 'The window was moved to workspace 1');

kill_all_windows;

###############################################################################
# Upon managing a window which sets _NET_WM_DESKTOP to the appropriate value,
# the window is made sticky and floating.
###############################################################################

cmd 'workspace 0';
$con = open_window_with_net_wm_desktop(0xFFFFFFFF);

is(get_net_wm_desktop($con), 0xFFFFFFFF, '_NET_WM_DESKTOP still has the correct value');
is(@{get_ws('0')->{floating_nodes}}, 1, 'The window is floating');
ok(get_ws('0')->{floating_nodes}->[0]->{nodes}->[0]->{sticky}, 'The window is sticky');

kill_all_windows;

###############################################################################
# _NET_WM_DESKTOP is updated when the window is moved to another workspace
# on the same output.
###############################################################################

cmd 'workspace 0';
open_window;
cmd 'workspace 1';
open_window;
cmd 'workspace 0';
$con = open_window;

cmd 'move window to workspace 1';

is(get_net_wm_desktop($con), 1, '_NET_WM_DESKTOP is updated when moving the window');

kill_all_windows;

###############################################################################
# _NET_WM_DESKTOP is updated when the floating window is moved to another
# workspace on the same output.
###############################################################################

cmd 'workspace 0';
open_window;
cmd 'workspace 1';
open_window;
cmd 'workspace 0';
$con = open_window;
cmd 'floating enable';

cmd 'move window to workspace 1';

is(get_net_wm_desktop($con), 1, '_NET_WM_DESKTOP is updated when moving the window');

kill_all_windows;

###############################################################################
# _NET_WM_DESKTOP is removed when the window is withdrawn.
###############################################################################

$con = open_window;
is(get_net_wm_desktop($con), 0, '_NET_WM_DESKTOP is set (sanity check)');

$con->unmap;
wait_for_unmap($con);

is(get_net_wm_desktop($con), undef, '_NET_WM_DESKTOP is removed');

kill_all_windows;

###############################################################################
# A _NET_WM_DESKTOP client message sent to the root window moves a window
# to the correct workspace.
###############################################################################

cmd 'workspace 0';
open_window;
cmd 'workspace 1';
open_window;
cmd 'workspace 0';

$con = open_window;
is_num_children('0', 2, 'The window is on workspace 0');

send_net_wm_desktop($con, 1);

is_num_children('0', 1, 'The window is no longer on workspace 0');
is_num_children('1', 2, 'The window is now on workspace 1');
is(get_net_wm_desktop($con), 1, '_NET_WM_DESKTOP is updated');

kill_all_windows;

###############################################################################
# A _NET_WM_DESKTOP client message sent to the root window can make a window
# sticky.
###############################################################################

cmd 'workspace 0';
$con = open_window;

send_net_wm_desktop($con, 0xFFFFFFFF);

is(get_net_wm_desktop($con), 0xFFFFFFFF, '_NET_WM_DESKTOP is updated');
is(@{get_ws('0')->{floating_nodes}}, 1, 'The window is floating');
ok(get_ws('0')->{floating_nodes}->[0]->{nodes}->[0]->{sticky}, 'The window is sticky');

kill_all_windows;

###############################################################################
# _NET_WM_DESKTOP is updated when a new workspace with a lower number is
# opened and closed.
###############################################################################

cmd 'workspace 1';
$con = open_window;
is(get_net_wm_desktop($con), 0, '_NET_WM_DESKTOP is set sanity check)');

cmd 'workspace 0';
is(get_net_wm_desktop($con), 1, '_NET_WM_DESKTOP is updated');

kill_all_windows;

###############################################################################
# _NET_WM_DESKTOP is updated when a window is made sticky by command.
###############################################################################

cmd 'workspace 0';
$con = open_window;
cmd 'floating enable';
is(get_net_wm_desktop($con), 0, '_NET_WM_DESKTOP is set sanity check)');

cmd 'sticky enable';
is(get_net_wm_desktop($con), 0xFFFFFFFF, '_NET_WM_DESKTOP is updated');

kill_all_windows;

###############################################################################
# _NET_WM_DESKTOP is updated when a window is made sticky by client message.
###############################################################################

cmd 'workspace 0';
$con = open_window;
cmd 'floating enable';
is(get_net_wm_desktop($con), 0, '_NET_WM_DESKTOP is set sanity check)');

my $msg = pack "CCSLLLLLL",
    X11::XCB::CLIENT_MESSAGE, 32, 0,
    $con->{id},
    $x->atom(name => '_NET_WM_STATE')->id,
    1,
    $x->atom(name => '_NET_WM_STATE_STICKY')->id,
    0, 0, 0;

$x->send_event(0, $x->get_root_window(), X11::XCB::EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);
sync_with_i3;

is(get_net_wm_desktop($con), 0xFFFFFFFF, '_NET_WM_DESKTOP is updated');

kill_all_windows;

###############################################################################
# _NET_WM_DESKTOP is updated when a window is moved to the scratchpad.
###############################################################################

cmd 'workspace 0';
$con = open_window;
cmd 'floating enable';
is(get_net_wm_desktop($con), 0, '_NET_WM_DESKTOP is set sanity check)');

cmd 'move scratchpad';
is(get_net_wm_desktop($con), 0xFFFFFFFF, '_NET_WM_DESKTOP is updated');

cmd 'scratchpad show';
is(get_net_wm_desktop($con), 0, '_NET_WM_DESKTOP is set sanity check)');

kill_all_windows;

###############################################################################

done_testing;
