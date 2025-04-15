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
# Tests if the urgency hint will be set appropriately when opening a window
# assigned to a workspace.
#
use i3test i3_autostart => 0;

# Based on the eponymous function in t/166-assign.t
sub open_special {
    my %args = @_;
    $args{name} //= 'special window';

    # We use dont_map because i3 will not map the window on the current
    # workspace. Thus, open_window would time out in wait_for_map (2 seconds).
    my $window = open_window(
        %args,
        wm_class => 'special',
        dont_map => 1,
    );
    $window->map;
    return $window;
}

#####################################################################
# start a window assigned to a non-visible workspace and see that the urgency
# hint is set.
#####################################################################

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
assign [class="special"] → targetws
EOT

my $pid = launch_with_config($config);

cmd 'workspace ordinaryws';
my $window = open_special;
sync_with_i3;

ok(get_ws('targetws')->{urgent}, 'target workspace is urgent');

$window->destroy;

exit_gracefully($pid);


#####################################################################
# start a window assigned to a visible workspace and see that the urgency hint
# is not set.
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
assign [class="special"] → targetws
EOT

$pid = launch_with_config($config);

cmd 'workspace targetws';
$window = open_special;
sync_with_i3;

ok(!get_ws('targetws')->{urgent}, 'visible workspace is not urgent');

$window->destroy;

exit_gracefully($pid);

#####################################################################
# start a window assigned to a visible workspace on a different output and see
# that the urgency hint is not set.
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
workspace targetws output fake-0
workspace ordinaryws output fake-1

assign [class="special"] → targetws
EOT

$pid = launch_with_config($config);

cmd 'workspace ordinaryws';
$window = open_special;
sync_with_i3;

ok(!get_ws('targetws')->{urgent}, 'target workspace is not urgent');

$window->destroy;

exit_gracefully($pid);

done_testing;
