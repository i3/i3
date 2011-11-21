#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
# Tests if assignments work
#
use i3test;
use X11::XCB qw(PROP_MODE_REPLACE WINDOW_CLASS_INPUT_OUTPUT);

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

my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
    event_mask => [ 'structure_notify' ],
);

$window->_create;
set_wm_class($window->id, 'special', 'special');
$window->name('special window');
$window->map;
wait_for_map $window;

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

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
    event_mask => [ 'structure_notify' ],
);

$window->_create;
set_wm_class($window->id, 'special', 'special');
$window->name('special window');
$window->map;
wait_for_map $window;

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

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
    event_mask => [ 'structure_notify' ],
);

$window->_create;
set_wm_class($window->id, 'special', 'special');
$window->name('special window');
$window->map;

# We use sync_with_i3 instead of wait_for_map here because i3 will not actually
# map the window -- it will be assigned to a different workspace and will only
# be mapped once you switch to that workspace
sync_with_i3 $x;

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

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
    event_mask => [ 'structure_notify' ],
);

$window->_create;
set_wm_class($window->id, 'special', 'special');
$window->name('special window');
$window->map;
wait_for_map $window;

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

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
    event_mask => [ 'structure_notify' ],
);

$window->_create;
set_wm_class($window->id, 'SPEcial', 'SPEcial');
$window->name('special window');
$window->map;
wait_for_map $window;

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

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
assign "special" → ~
EOT

$pid = launch_with_config($config);

# TODO: replace this with checking the process hierarchy
# XXX: give i3-nagbar some time to start up
sleep 1;

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
my @docked = get_dock_clients;
# We expect i3-nagbar as the first dock client due to using the old assign
# syntax
is(@docked, 1, 'one dock client yet');

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    event_mask => [ 'structure_notify' ],
);

$window->_create;
set_wm_class($window->id, 'special', 'special');
$window->name('special window');
$window->map;
wait_for_map $window;

$content = get_ws($tmp);
ok(@{$content->{nodes}} == 0, 'no tiling cons');
ok(@{$content->{floating_nodes}} == 0, 'one floating con');
@docked = get_dock_clients;
is(@docked, 2, 'two dock clients now');

$window->destroy;

does_i3_live;

exit_gracefully($pid);

sleep 0.25;

done_testing;
