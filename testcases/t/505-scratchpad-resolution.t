#!perl
# vim:ts=4:sw=4:expandtab
#
# Verifies that scratchpad windows don’t move due to floating point caulcation
# errors when repeatedly hiding/showing, no matter what display resolution.
#
use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 683x768+0+0,1024x768+683+0
EOT
my $pid = launch_with_config($config);

my $i3 = i3(get_socket_path());

$x->root->warp_pointer(0, 0);
sync_with_i3;

sub verify_scratchpad_doesnt_move {
    my ($ws) = @_;

    is_num_children($ws, 0, 'no nodes on this ws');

    my $window = open_window;
    is_num_children($ws, 1, 'one node on this ws');

    cmd 'move scratchpad';
    is_num_children($ws, 0, 'no nodes on this ws');

    my $last_x = -1;
    for (1 .. 20) {
        cmd 'scratchpad show';
        is(scalar @{get_ws($ws)->{floating_nodes}}, 1, 'one floating node on this ws');

        # Verify that the coordinates are within bounds.
        my $content = get_ws($ws);
        my $srect = $content->{floating_nodes}->[0]->{rect};
        if ($last_x > -1) {
            is($srect->{x}, $last_x, 'scratchpad window did not move');
        }
        $last_x = $srect->{x};
        cmd 'scratchpad show';
    }

    # We need to kill the scratchpad window, otherwise scratchpad show in
    # subsequent calls of verify_scratchpad_doesnt_move will cycle between all
    # the windows.
    cmd 'scratchpad show';
    cmd 'kill';
}

################################################################################
# test it on the left output first (1366x768)
################################################################################

my $second = fresh_workspace(output => 0);
verify_scratchpad_doesnt_move($second);

################################################################################
# now on the right output (1024x768)
################################################################################

$x->root->warp_pointer(683 + 10, 0);
sync_with_i3;

my $third = fresh_workspace(output => 1);
verify_scratchpad_doesnt_move($third);

exit_gracefully($pid);

done_testing;
