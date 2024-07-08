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
# Tests for _NET_MOVERESIZE_WINDOW.
# Ticket: #2603
use i3test i3_autostart => 0;

sub moveresize_window {
    my ($win, $pos_x, $pos_y, $width, $height) = @_;

    my $flags = 0;
    $flags |= (1 << 8) if $pos_x >= 0;
    $flags |= (1 << 9) if $pos_y >= 0;
    $flags |= (1 << 10) if $width >= 0;
    $flags |= (1 << 11) if $height >= 0;

    my $msg = pack "CCSLLLLLLL",
        X11::XCB::CLIENT_MESSAGE, # response_type
        32, # format
        0, # sequence
        $win->id, # window
        $x->atom(name => '_NET_MOVERESIZE_WINDOW')->id, # message type
        $flags, # data32[0] (flags)
        $pos_x, # data32[1] (x)
        $pos_y, # data32[2] (y)
        $width, # data32[3] (width)
        $height; # data32[4] (height)

    $x->send_event(0, $x->get_root_window(), X11::XCB::EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);
    sync_with_i3;
}

my $config = <<EOT;
font font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_window none
new_float none
EOT

my ($pid, $ws, $window, $content);

###############################################################################

###############################################################################
# A _NET_MOVERESIZE_WINDOW client message can change the position and size
# of a floating window.
###############################################################################

$pid = launch_with_config($config);
$ws = fresh_workspace;

$window = open_floating_window(rect => [50, 50, 100, 100]);
moveresize_window($window, 0, 0, 555, 666);

$content = get_ws($ws);
is($content->{floating_nodes}->[0]->{rect}->{x}, 0, 'the x coordinate is correct');
is($content->{floating_nodes}->[0]->{rect}->{y}, 0, 'the y coordinate is correct');
is($content->{floating_nodes}->[0]->{rect}->{width}, 555, 'the width is correct');
is($content->{floating_nodes}->[0]->{rect}->{height}, 666, 'the height is correct');

exit_gracefully($pid);

###############################################################################
# A _NET_MOVERESIZE_WINDOW client message can change only the position of a
# window.
###############################################################################

$pid = launch_with_config($config);
$ws = fresh_workspace;

$window = open_floating_window(rect => [50, 50, 100, 100]);
moveresize_window($window, 100, 100, -1, -1);

$content = get_ws($ws);
is($content->{floating_nodes}->[0]->{rect}->{x}, 100, 'the x coordinate is correct');
is($content->{floating_nodes}->[0]->{rect}->{y}, 100, 'the y coordinate is correct');
is($content->{floating_nodes}->[0]->{rect}->{width}, 100, 'the width is unchanged');
is($content->{floating_nodes}->[0]->{rect}->{height}, 100, 'the height is unchanged');

exit_gracefully($pid);

###############################################################################
# A _NET_MOVERESIZE_WINDOW client message can change only the size of a
# window.
###############################################################################

$pid = launch_with_config($config);
$ws = fresh_workspace;

$window = open_floating_window(rect => [50, 50, 100, 100]);
moveresize_window($window, -1, -1, 200, 200);

$content = get_ws($ws);
is($content->{floating_nodes}->[0]->{rect}->{x}, 50, 'the x coordinate is unchanged');
is($content->{floating_nodes}->[0]->{rect}->{y}, 50, 'the y coordinate is unchanged');
is($content->{floating_nodes}->[0]->{rect}->{width}, 200, 'the width is correct');
is($content->{floating_nodes}->[0]->{rect}->{height}, 200, 'the height is correct');

exit_gracefully($pid);

###############################################################################

done_testing;
