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
# Tests for the focus_on_window_activation directive
# Ticket: #1426
use i3test i3_autostart => 0;
use List::Util qw(first);

my ($config, $pid, $first, $second, $ws1, $ws2);

sub send_net_active_window {
    my ($id) = @_; 

    my $msg = pack "CCSLLLLLLL",
        X11::XCB::CLIENT_MESSAGE, # response_type
        32, # format
        0, # sequence
        $id, # destination window
        $x->atom(name => '_NET_ACTIVE_WINDOW')->id,
        0, # source
        0, 0, 0, 0;

    $x->send_event(0, $x->get_root_window(), X11::XCB::EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);
}

sub get_urgency_for_window_on_workspace {
    my ($ws, $con) = @_;

    my $current = first { $_->{window} == $con->{id} } @{get_ws_content($ws)};
    return $current->{urgent};
}

#####################################################################
# 1: If mode is set to 'urgent' and the target workspace is visible,
#    check that the urgent flag is set and focus is not lost.
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

focus_on_window_activation urgent
EOT

$pid = launch_with_config($config);

my $ws = fresh_workspace;
$first = open_window;
$second = open_window;

send_net_active_window($first->id);
sync_with_i3;

is($x->input_focus, $second->id, 'second window is still focused');
is(get_urgency_for_window_on_workspace($ws, $first), 1, 'first window is marked urgent');

exit_gracefully($pid);

#####################################################################
# 2: If mode is set to 'urgent' and the target workspace is not
#    visible, check that the urgent flag is set and focus is not lost.
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

focus_on_window_activation urgent
EOT

$pid = launch_with_config($config);

$ws1 = fresh_workspace;
$first = open_window;
$ws2 = fresh_workspace;
$second = open_window;

send_net_active_window($first->id);
sync_with_i3;

is(focused_ws(), $ws2, 'second workspace is still focused');
is($x->input_focus, $second->id, 'second window is still focused');
is(get_urgency_for_window_on_workspace($ws1, $first), 1, 'first window is marked urgent');

exit_gracefully($pid);

#####################################################################
# 3: If mode is set to 'focus' and the target workspace is visible,
#    check that the focus is switched.
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

focus_on_window_activation focus
EOT

$pid = launch_with_config($config);

$ws = fresh_workspace;
$first = open_window;
$second = open_window;

send_net_active_window($first->id);
sync_with_i3;

is($x->input_focus, $first->id, 'first window is now focused');
ok(!get_urgency_for_window_on_workspace($ws, $first), 'first window is not marked urgent');

exit_gracefully($pid);

#####################################################################
# 4: If mode is set to 'focus' and the target workspace is not
#    visible, check that the focus switched.
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

focus_on_window_activation focus
EOT

$pid = launch_with_config($config);

$ws1 = fresh_workspace;
$first = open_window;
$ws2 = fresh_workspace;
$second = open_window;

send_net_active_window($first->id);
sync_with_i3;

is(focused_ws(), $ws1, 'first workspace is now focused');
is($x->input_focus, $first->id, 'first window is now focused');
ok(!get_urgency_for_window_on_workspace($ws1, $first), 'first window is not marked urgent');

exit_gracefully($pid);

#####################################################################
# 5: If mode is set to 'none' and the target workspace is visible,
#    check that nothing happens.
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

focus_on_window_activation none
EOT

$pid = launch_with_config($config);

$ws = fresh_workspace;
$first = open_window;
$second = open_window;

send_net_active_window($first->id);
sync_with_i3;

is($x->input_focus, $second->id, 'second window is still focused');
ok(!get_urgency_for_window_on_workspace($ws, $first), 'first window is not marked urgent');

exit_gracefully($pid);

#####################################################################
# 6: If mode is set to 'none' and the target workspace is not
#    visible, check that nothing happens.
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

focus_on_window_activation none
EOT

$pid = launch_with_config($config);

$ws1 = fresh_workspace;
$first = open_window;
$ws2 = fresh_workspace;
$second = open_window;

send_net_active_window($first->id);
sync_with_i3;

is(focused_ws(), $ws2, 'second workspace is still focused');
is($x->input_focus, $second->id, 'second window is still focused');
ok(!get_urgency_for_window_on_workspace($ws1, $first), 'first window is not marked urgent');

exit_gracefully($pid);

done_testing;
