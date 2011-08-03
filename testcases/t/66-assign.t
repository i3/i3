#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
# Tests if assignments work
#
use i3test;
use X11::XCB qw(:all);
use X11::XCB::Connection;
use v5.10;

my $x = X11::XCB::Connection->new;

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

my $process = launch_with_config($config);

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
);

$window->_create;
set_wm_class($window->id, 'special', 'special');
$window->name('special window');
$window->map;
sleep 0.25;

ok(@{get_ws_content($tmp)} == 1, 'special window got managed to current (random) workspace');

exit_gracefully($process->pid);

$window->destroy;

sleep 0.25;

#####################################################################
# start a window and see that it gets assigned to a formerly unused
# workspace
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
assign "special" → targetws
EOT

$process = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
my $workspaces = get_workspace_names;
ok(!("targetws" ~~ @{$workspaces}), 'targetws does not exist yet');

my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
);

$window->_create;
set_wm_class($window->id, 'special', 'special');
$window->name('special window');
$window->map;
sleep 0.25;

ok(@{get_ws_content($tmp)} == 0, 'still no containers');
ok("targetws" ~~ @{get_workspace_names()}, 'targetws exists');

$window->destroy;

exit_gracefully($process->pid);

sleep 0.25;

#####################################################################
# start a window and see that it gets assigned to a workspace which has content
# already, next to the existing node.
#####################################################################

$process = launch_with_config($config);

# initialize the target workspace, then go to a fresh one
ok(!("targetws" ~~ @{get_workspace_names()}), 'targetws does not exist yet');
cmd 'workspace targetws';
cmp_ok(@{get_ws_content('targetws')}, '==', 0, 'no containers on targetws yet');
cmd 'open';
cmp_ok(@{get_ws_content('targetws')}, '==', 1, 'one container on targetws');
$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
ok("targetws" ~~ @{get_workspace_names()}, 'targetws does not exist yet');

my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
);

$window->_create;
set_wm_class($window->id, 'special', 'special');
$window->name('special window');
$window->map;
sleep 0.25;

ok(@{get_ws_content($tmp)} == 0, 'still no containers');
ok(@{get_ws_content('targetws')} == 2, 'two containers on targetws');

exit_gracefully($process->pid);

#####################################################################
# start a window and see that it gets assigned to a workspace which has content
# already, next to the existing node.
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
assign "special" → ~
EOT

$process = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
my $workspaces = get_workspace_names;
ok(!("targetws" ~~ @{$workspaces}), 'targetws does not exist yet');

my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
);

$window->_create;
set_wm_class($window->id, 'special', 'special');
$window->name('special window');
$window->map;
sleep 0.25;

my $content = get_ws($tmp);
ok(@{$content->{nodes}} == 0, 'no tiling cons');
ok(@{$content->{floating_nodes}} == 1, 'one floating con');

$window->destroy;

exit_gracefully($process->pid);

sleep 0.25;

done_testing;
