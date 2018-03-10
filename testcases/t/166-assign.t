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
# Tests if assignments work
#
use i3test i3_autostart => 0;

sub open_special {
    my %args = @_;
    $args{name} //= 'special window';
    $args{wm_class} //= 'special';

    # We use dont_map because i3 will not map the window on the current
    # workspace. Thus, open_window would time out in wait_for_map (2 seconds).
    my $window = open_window(
        %args,
        dont_map => 1,
    );
    $window->map;
    return $window;
}

sub test_workspace_assignment {
    my $target_ws = "@_";

    # initialize the target workspace, then go to a fresh one
    ok(!($target_ws ~~ @{get_workspace_names()}), "$target_ws does not exist yet");
    cmd "workspace $target_ws";
    cmp_ok(@{get_ws_content($target_ws)}, '==', 0, "no containers on $target_ws yet");
    cmd 'open';
    cmp_ok(@{get_ws_content($target_ws)}, '==', 1, "one container on $target_ws");
    my $tmp = fresh_workspace;

    ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
    ok($target_ws ~~ @{get_workspace_names()}, "$target_ws does not exist yet");

    # We use sync_with_i3 instead of wait_for_map here because i3 will not actually
    # map the window -- it will be assigned to a different workspace and will only
    # be mapped once you switch to that workspace
    my $window = open_special;
    sync_with_i3;

    ok(@{get_ws_content($tmp)} == 0, 'still no containers');
    ok(@{get_ws_content($target_ws)} == 2, "two containers on $target_ws");

    return $window
}

#####################################################################
# start a window and see that it does not get assigned with an empty config
#####################################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $pid = launch_with_config($config);

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $window = open_special;
wait_for_map($window);

ok(@{get_ws_content($tmp)} == 1, 'special window got managed to current (random) workspace');

exit_gracefully($pid);

$window->destroy;

#####################################################################
# start a window and see that it gets assigned to a formerly unused
# workspace
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
assign [class="special"] → targetws
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
my $workspaces = get_workspace_names;
ok(!("targetws" ~~ @{$workspaces}), 'targetws does not exist yet');

$window = open_special;
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'still no containers');
ok("targetws" ~~ @{get_workspace_names()}, 'targetws exists');

$window->destroy;

exit_gracefully($pid);

#####################################################################
# start a window and see that it gets assigned to a formerly unused
# numbered workspace
#####################################################################

my $config_numbered = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
assign [class="special"] → workspace number 2
EOT

$pid = launch_with_config($config_numbered);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
$workspaces = get_workspace_names;
ok(!("2" ~~ @{$workspaces}), 'workspace number 2 does not exist yet');

$window = open_special;
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'still no containers');
ok("2" ~~ @{get_workspace_names()}, 'workspace number 2 exists');

$window->destroy;

exit_gracefully($pid);

#####################################################################
# start a window and see that it gets assigned to a numbered
# workspace which has content already, next to the existing node.
#####################################################################

$pid = launch_with_config($config_numbered);

$window = test_workspace_assignment("2");
$window->destroy;

exit_gracefully($pid);

#####################################################################
# start a window and see that it gets assigned to a numbered workspace with
# a name which has content already, next to the existing node.
#####################################################################

$pid = launch_with_config($config_numbered);

cmd 'workspace 2';  # Make sure that we are not testing for "2" again.
$window = test_workspace_assignment("2: targetws");
$window->destroy;

exit_gracefully($pid);

#####################################################################
# start a window and see that it gets assigned to a workspace which
# has content already, next to the existing node.
#####################################################################

$pid = launch_with_config($config);

test_workspace_assignment("targetws");

exit_gracefully($pid);

#####################################################################
# start a window and see that it gets assigned to a workspace which has content
# already, next to the existing node.
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [class="special"] floating enable
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
$workspaces = get_workspace_names;
ok(!("targetws" ~~ @{$workspaces}), 'targetws does not exist yet');

$window = open_special;
sync_with_i3;

my $content = get_ws($tmp);
ok(@{$content->{nodes}} == 0, 'no tiling cons');
ok(@{$content->{floating_nodes}} == 1, 'one floating con');

kill_all_windows;
exit_gracefully($pid);

#####################################################################
# test assignments to named outputs
#####################################################################
$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0,1024x768+1024+768,1024x768+0+768

workspace ws-0 output fake-0
workspace ws-1 output fake-1
workspace ws-2 output fake-2
workspace ws-3 output fake-3

assign [class="special-0"] → output fake-0
assign [class="special-1"] → output fake-1
assign [class="special-2"] → output fake-2
assign [class="special-3"] → output fake-3
assign [class="special-4"] → output invalid

EOT

$pid = launch_with_config($config);

sub open_in_output {
    my ($num, $expected_count) = @_;
    my $ws = "ws-$num";
    my $class = "special-$num";
    my $output = "fake-$num";

    is_num_children($ws, $expected_count - 1,
                    "before: " . ($expected_count - 1) . " containers on output $output");
    $window = open_special(wm_class => $class);
    sync_with_i3;
    is_num_children($ws, $expected_count,
                    "after: $expected_count containers on output $output");
}

cmd "workspace ws-0";
open_in_output(0, 1);
my $focused = $x->input_focus;

open_in_output(1, 1);
is($x->input_focus, $focused, 'focus remains on output fake-0');

open_in_output(2, 1);
is($x->input_focus, $focused, 'focus remains on output fake-0');

for my $i (1 .. 5){
    open_in_output(3, $i);
    is($x->input_focus, $focused, 'focus remains on output fake-0');
}

# Check invalid output
$tmp = fresh_workspace;
open_special(wm_class => "special-4");
sync_with_i3;
is_num_children($tmp, 1, 'window assigned to invalid output opened in current workspace');
open_special(wm_class => "special-3");
sync_with_i3;
is_num_children($tmp, 1, 'but window assigned to valid output did not');

kill_all_windows;
exit_gracefully($pid);

#####################################################################
# Test assignments to outputs with relative names
#####################################################################
$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0,1024x768+1024+768,1024x768+0+768

workspace left-top output fake-0
workspace right-top output fake-1
workspace right-bottom output fake-2
workspace left-bottom output fake-3

assign [class="current"] → output current
assign [class="left"] → output left
assign [class="right"] → output right
assign [class="up"] → output up
assign [class="down"] → output down
EOT

$pid = launch_with_config($config);

cmd 'workspace left-top';

is_num_children('left-top', 0, 'no childreon on left-top');
for my $i (1 .. 5){
    open_special(wm_class => 'current');
}
sync_with_i3;
is_num_children('left-top', 5, 'windows opened in current workspace');

is_num_children('right-top', 0, 'no children on right-top');
open_special(wm_class => 'right');
sync_with_i3;
is_num_children('right-top', 1, 'one child on right-top');

is_num_children('left-bottom', 0, 'no children on left-bottom');
open_special(wm_class => 'down');
sync_with_i3;
is_num_children('left-bottom', 1, 'one child on left-bottom');

cmd 'workspace right-bottom';

open_special(wm_class => 'up');
sync_with_i3;
is_num_children('right-top', 2, 'two children on right-top');

open_special(wm_class => 'left');
sync_with_i3;
is_num_children('left-bottom', 2, 'two children on left-bottom');

kill_all_windows;
exit_gracefully($pid);

#####################################################################
# regression test: dock clients with floating assignments should not crash
# (instead, nothing should happen - dock clients can’t float)
# ticket #501
#####################################################################

# Walks /proc to figure out whether a child process of $i3pid with the name
# 'i3-nagbar' exists.
sub i3nagbar_running {
    my ($i3pid) = @_;

    my @procfiles = grep { m,^/proc/[0-9]+$, } </proc/*>;
    for my $path (@procfiles) {
        open(my $fh, '<', "$path/stat") or next;
        my $line = <$fh>;
        close($fh);
        my ($comm, $ppid) = ($line =~ /^[0-9]+ \(([^)]+)\) . ([0-9]+)/);
        return 1 if $ppid == $i3pid && $comm eq 'i3-nagbar';
    }
    return 0;
}

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [title="special"] floating enable
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
my @docked = get_dock_clients;
is(@docked, 0, 'no dock client yet');

$window = open_special(
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
);
sync_with_i3;

$content = get_ws($tmp);
ok(@{$content->{nodes}} == 0, 'no tiling cons');
ok(@{$content->{floating_nodes}} == 0, 'one floating con');
@docked = get_dock_clients;
is(@docked, 1, 'one dock client now');

$window->destroy;

does_i3_live;

exit_gracefully($pid);

done_testing;
