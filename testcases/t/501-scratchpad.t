#!perl
# vim:ts=4:sw=4:expandtab
#
# Verifies that scratchpad windows show up on the proper output.
# ticket #596, bug present until up to commit
# 89dded044b4fffe78f9d70778748fabb7ac533e9.
#
use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT
my $pid = launch_with_config($config);

my $i3 = i3(get_socket_path());

################################################################################
# Open a workspace on the second output, put a window to scratchpad, display
# it, verify it’s on the same workspace.
################################################################################

sub verify_scratchpad_on_same_ws {
    my ($ws) = @_;

    is_num_children($ws, 0, 'no nodes on this ws');

    my $window = open_window;

    is_num_children($ws, 1, 'one nodes on this ws');

    cmd 'move scratchpad';

    is_num_children($ws, 0, 'no nodes on this ws');

    cmd 'scratchpad show';
    is_num_children($ws, 0, 'no nodes on this ws');
    is(scalar @{get_ws($ws)->{floating_nodes}}, 1, 'one floating node on this ws');
}

my $second = fresh_workspace(output => 1);

verify_scratchpad_on_same_ws($second);

################################################################################
# The same thing, but on the first output.
################################################################################

my $first = fresh_workspace(output => 0);

verify_scratchpad_on_same_ws($first);

################################################################################
# Now open the scratchpad on one output and switch to another.
################################################################################

sub verify_scratchpad_switch {
    my ($first, $second) = @_;

    cmd "workspace $first";

    is_num_children($first, 0, 'no nodes on this ws');

    my $window = open_window;

    is_num_children($first, 1, 'one nodes on this ws');

    cmd 'move scratchpad';

    is_num_children($first, 0, 'no nodes on this ws');

    cmd "workspace $second";

    cmd 'scratchpad show';
    my $ws = get_ws($second);
    is_num_children($second, 0, 'no nodes on this ws');
    is(scalar @{$ws->{floating_nodes}}, 1, 'one floating node on this ws');

    # Verify that the coordinates are within bounds.
    my $srect = $ws->{floating_nodes}->[0]->{rect};
    my $rect = $ws->{rect};
    cmd 'nop before bounds check';
    cmp_ok($srect->{x}, '>=', $rect->{x}, 'x within bounds');
    cmp_ok($srect->{y}, '>=', $rect->{y}, 'y within bounds');
    cmp_ok($srect->{x} + $srect->{width}, '<=', $rect->{x} + $rect->{width},
           'width within bounds');
    cmp_ok($srect->{y} + $srect->{height}, '<=', $rect->{y} + $rect->{height},
           'height within bounds');
}

$first = fresh_workspace(output => 0);
$second = fresh_workspace(output => 1);

verify_scratchpad_switch($first, $second);

$first = fresh_workspace(output => 1);
$second = fresh_workspace(output => 0);

verify_scratchpad_switch($first, $second);

exit_gracefully($pid);

done_testing;
