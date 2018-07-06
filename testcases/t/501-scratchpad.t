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
# Verifies that scratchpad windows show up on the proper output.
# ticket #596, bug present until up to commit
# 89dded044b4fffe78f9d70778748fabb7ac533e9.
#
use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

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

################################################################################
# Test 'scratchpad show [on] output <name>'
################################################################################

sub is_num_floating_children {
    my ($ws, $num, $msg) = @_;
    is(scalar @{get_ws($ws)->{floating_nodes}}, $num, $msg);
}

sub verify_show_on_output {
    my ($outa, $outb) = @_;

    my $out_a = "fake-$outa";
    my $out_b = "fake-$outb";

    # open window in first workspace
    my $first_ws = fresh_workspace(output => $outa);
    cmd "workspace $first_ws";

    my $sp_window = open_window(name=>'Scratchpad');
    my $sp_con = get_focused($first_ws);

    is_num_children($first_ws, 1, 'scratchpad window created');

    # test moving to scratchpad works
    cmd "move scratchpad";

    is_num_children($first_ws, 0, 'window moved to scratchpad');

    # test that 'show'ing it from the same workspace works fine
    cmd "scratchpad show on output $out_a";

    is(focused_ws, $first_ws, 'workspace at output a is focused');
    is(get_focused($first_ws), $sp_con, 'scratchpad is focused');

    # check that executing the command with the scratchpad window focused
    # hides the window
    cmd "scratchpad show on output $out_a";

    is_num_children($first_ws, 0, 'no node on first workspace');
    is_num_floating_children($first_ws, 0, 'no floating node on workspace');

    # focus workspace on output b
    my $second_ws = fresh_workspace(output => $outb);
    cmd "workspace $second_ws";

    is(focused_ws, $second_ws, 'second workspace is focused');

    # check that showing scratchpad windows adds windows to workspace and
    # focuses them
    cmd "scratchpad show on output $out_a";

    is(focused_ws, $first_ws, 'workspace at output a is focused');
    is(get_focused($first_ws), $sp_con, 'scratchpad window focused');

    # go back to second workspace and check that 'show'ing brings focus
    # back to the window
    cmd "workspace $second_ws";

    is(focused_ws, $second_ws, 'second workspace is focused');

    cmd "scratchpad show on output $out_a";

    is(focused_ws, $first_ws, 'workspace at output a is focused');
    is(get_focused($first_ws), $sp_con, 'scratchpad window focused');

    # go to a new workspace on the same output and make sure 'show'ing
    # the window moves it up to this workspace
    my $third_ws = fresh_workspace(output => $outa);
    cmd "workspace $third_ws";

    is(focused_ws, $third_ws, 'third workspace is focused');

    cmd "scratchpad show on output $out_a";

    is(focused_ws, $third_ws, 'workspace three is focused');
    is(get_focused($third_ws), $sp_con, 'scratchpad window focused');

    # go back to second workspace and check that '--hide-if-visible'
    # sends the window back to scratchpad
    cmd "workspace $second_ws";

    is(focused_ws, $second_ws, 'second workspace is focused');


    cmd "scratchpad show --hide-if-visible on output $out_a";

    is_num_floating_children($third_ws, 0, 'no floating child for third workspace');
    is_num_children($third_ws, 0, 'no child for third workspace');
}

kill_all_windows;
verify_show_on_output(0, 1);

kill_all_windows;
verify_show_on_output(1, 0);

done_testing;
