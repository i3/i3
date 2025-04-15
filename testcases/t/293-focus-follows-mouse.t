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
# Tests the focus_follows_mouse setting.
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1000x1000+0+0
EOT

my ($first, $second);

sub synced_warp_pointer {
    my ($x_px, $y_px) = @_;
    sync_with_i3;
    $x->root->warp_pointer($x_px, $y_px);
    sync_with_i3;
}

###################################################################
# Test a simple case with 2 windows.
###################################################################

synced_warp_pointer(600, 600);
$first = open_window;
$second = open_window;
is($x->input_focus, $second->id, 'second window focused');

synced_warp_pointer(0, 0);
is($x->input_focus, $first->id, 'first window focused');

###################################################################
# Test that focus isn't changed with tabbed windows.
###################################################################

fresh_workspace;
synced_warp_pointer(600, 600);
$first = open_window;
cmd 'layout tabbed';
$second = open_window;
is($x->input_focus, $second->id, 'second (tabbed) window focused');

synced_warp_pointer(0, 0);
is($x->input_focus, $second->id, 'second window still focused');

###################################################################
# Test that floating windows are focused but not raised to the top.
# See issue #2990.
###################################################################

my $ws;
my $tmp = fresh_workspace;
my ($first_floating, $second_floating);

synced_warp_pointer(0, 0);
$first_floating = open_floating_window(rect => [ 1, 1, 100, 100 ]);
$second_floating = open_floating_window(rect => [ 50, 50, 100, 100 ]);
$first = open_window;

is($x->input_focus, $first->id, 'first (tiling) window focused');
$ws = get_ws($tmp);
is($ws->{floating_nodes}->[1]->{nodes}->[0]->{window}, $second_floating->id, 'second floating on top');
is($ws->{floating_nodes}->[0]->{nodes}->[0]->{window}, $first_floating->id, 'first floating behind');

synced_warp_pointer(40, 40);
is($x->input_focus, $first_floating->id, 'first floating window focused');
$ws = get_ws($tmp);
is($ws->{floating_nodes}->[1]->{nodes}->[0]->{window}, $second_floating->id, 'second floating still on top');
is($ws->{floating_nodes}->[0]->{nodes}->[0]->{window}, $first_floating->id, 'first floating still behind');

done_testing;
