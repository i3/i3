#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
# Regression: Checks if focus is stolen when a window is managed which is
# assigned to an invisible workspace
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
assign "special" → targetws
EOT

my $process = launch_with_config($config);

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
ok(get_ws($tmp)->{focused}, 'current workspace focused');

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


ok(@{get_ws_content($tmp)} == 0, 'special window not on current workspace');
ok(@{get_ws_content('targetws')} == 1, 'special window on targetws');
ok(get_ws($tmp)->{focused}, 'current workspace still focused');

#####################################################################
# the same test, but with a floating window
#####################################################################

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
);

$window->_create;
set_wm_class($window->id, 'special', 'special');
$window->name('special window');
$window->map;
sleep 0.25;


ok(@{get_ws_content($tmp)} == 0, 'special window not on current workspace');
ok(@{get_ws_content('targetws')} == 1, 'special window on targetws');
ok(get_ws($tmp)->{focused}, 'current workspace still focused');

exit_gracefully($process->pid);

$window->destroy;

done_testing;
