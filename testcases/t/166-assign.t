#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests if assignments work
#
use i3test i3_autostart => 0;
use X11::XCB qw(PROP_MODE_REPLACE);

# TODO: move to X11::XCB
sub set_wm_class {
    my ($id, $class, $instance) = @_;

    # Add a _NET_WM_STRUT_PARTIAL hint
    my $atomname = $x->atom(name => 'WM_CLASS');
    my $atomtype = $x->atom(name => 'STRING');

    $x->change_property(
        PROP_MODE_REPLACE,
        $id,
        $atomname->id,
        $atomtype->id,
        8,
        length($class) + length($instance) + 2,
        "$instance\x00$class\x00"
    );
}

sub open_special {
    my %args = @_;
    my $wm_class = delete($args{wm_class}) || 'special';
    $args{name} //= 'special window';

    # We use dont_map because i3 will not map the window on the current
    # workspace. Thus, open_window would time out in wait_for_map (2 seconds).
    my $window = open_window(
        %args,
        before_map => sub { set_wm_class($_->id, $wm_class, $wm_class) },
        dont_map => 1,
    );
    $window->map;
    return $window;
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
assign "special" → targetws
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
my $workspaces = get_workspace_names;
ok(!("targetws" ~~ @{$workspaces}), 'targetws does not exist yet');

$window = open_special;

ok(@{get_ws_content($tmp)} == 0, 'still no containers');
ok("targetws" ~~ @{get_workspace_names()}, 'targetws exists');

$window->destroy;

exit_gracefully($pid);

sleep 0.25;

#####################################################################
# start a window and see that it gets assigned to a workspace which has content
# already, next to the existing node.
#####################################################################

$pid = launch_with_config($config);

# initialize the target workspace, then go to a fresh one
ok(!("targetws" ~~ @{get_workspace_names()}), 'targetws does not exist yet');
cmd 'workspace targetws';
cmp_ok(@{get_ws_content('targetws')}, '==', 0, 'no containers on targetws yet');
cmd 'open';
cmp_ok(@{get_ws_content('targetws')}, '==', 1, 'one container on targetws');
$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
ok("targetws" ~~ @{get_workspace_names()}, 'targetws does not exist yet');


# We use sync_with_i3 instead of wait_for_map here because i3 will not actually
# map the window -- it will be assigned to a different workspace and will only
# be mapped once you switch to that workspace
$window = open_special(dont_map => 1);
$window->map;
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'still no containers');
ok(@{get_ws_content('targetws')} == 2, 'two containers on targetws');

exit_gracefully($pid);

#####################################################################
# start a window and see that it gets assigned to a workspace which has content
# already, next to the existing node.
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
assign "special" → ~
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
$workspaces = get_workspace_names;
ok(!("targetws" ~~ @{$workspaces}), 'targetws does not exist yet');

$window = open_special;

my $content = get_ws($tmp);
ok(@{$content->{nodes}} == 0, 'no tiling cons');
ok(@{$content->{floating_nodes}} == 1, 'one floating con');

$window->destroy;

exit_gracefully($pid);

sleep 0.25;

#####################################################################
# make sure that assignments are case-insensitive in the old syntax.
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
assign "special" → ~
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
$workspaces = get_workspace_names;
ok(!("targetws" ~~ @{$workspaces}), 'targetws does not exist yet');

$window = open_special(wm_class => 'SPEcial');

$content = get_ws($tmp);
ok(@{$content->{nodes}} == 0, 'no tiling cons');
ok(@{$content->{floating_nodes}} == 1, 'one floating con');

$window->destroy;

exit_gracefully($pid);

sleep 0.25;

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
assign "special" → ~
EOT

$pid = launch_with_config($config);

# Ensure that i3-nagbar is running. It should be started pretty quickly, so we
# busy-loop with a short delay.
while (!i3nagbar_running($pid)) {
    sleep 0.05;
}

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
my @docked = get_dock_clients;
# We expect i3-nagbar as the first dock client due to using the old assign
# syntax
is(@docked, 1, 'one dock client yet');

$window = open_special(
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
);

$content = get_ws($tmp);
ok(@{$content->{nodes}} == 0, 'no tiling cons');
ok(@{$content->{floating_nodes}} == 0, 'one floating con');
@docked = get_dock_clients;
is(@docked, 2, 'two dock clients now');

$window->destroy;

does_i3_live;

exit_gracefully($pid);

done_testing;
