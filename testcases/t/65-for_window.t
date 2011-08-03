#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
#
use X11::XCB qw(:all);
use X11::XCB::Connection;
use i3test;

my $x = X11::XCB::Connection->new;

##############################################################
# 1: test the following directive:
#    for_window [class="borderless"] border none
# by first creating a window with a different class (should get
# the normal border), then creating a window with the class
# "borderless" (should get no border)
##############################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [class="borderless"] border none
for_window [title="special borderless title"] border none
EOT

my $process = launch_with_config($config);

my $tmp = fresh_workspace;

my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#00ff00',
);

$window->name('Border window');
$window->map;
sleep 0.25;

my @content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'normal', 'normal border');

$window->unmap;
sleep 0.25;

my @content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 0, 'no more nodes');
diag('content = '. Dumper(\@content));

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#00ff00',
);

$window->_create;

# TODO: move this to X11::XCB::Window
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

set_wm_class($window->id, 'borderless', 'borderless');
$window->name('Borderless window');
$window->map;
sleep 0.25;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'none', 'no border');

$window->unmap;
sleep 0.25;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 0, 'no more nodes');

exit_gracefully($process->pid);

##############################################################
# 2: match on the title, check if for_window is really executed
# only once
##############################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [class="borderless"] border none
for_window [title="special borderless title"] border none
EOT

$process = launch_with_config($config);

$tmp = fresh_workspace;

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#00ff00',
);

$window->name('special title');
$window->map;
sleep 0.25;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'normal', 'normal border');

$window->name('special borderless title');
sleep 0.25;

@content = @{get_ws_content($tmp)};
is($content[0]->{border}, 'none', 'no border');

$window->name('special title');
sleep 0.25;

cmd 'border normal';

@content = @{get_ws_content($tmp)};
is($content[0]->{border}, 'normal', 'border reset to normal');

$window->name('special borderless title');
sleep 0.25;

@content = @{get_ws_content($tmp)};
is($content[0]->{border}, 'normal', 'still normal border');

$window->unmap;
sleep 0.25;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 0, 'no more nodes');

exit_gracefully($process->pid);

##############################################################
# 3: match on the title, set border style *and* a mark
##############################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [class="borderless" title="usethis"] border none
for_window [class="borderless"] border none
for_window [title="special borderless title"] border none
for_window [title="special mark title"] border none, mark bleh
EOT

$process = launch_with_config($config);

$tmp = fresh_workspace;

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#00ff00',
);

$window->name('special mark title');
$window->map;
sleep 0.25;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'none', 'no border');

my $other = open_standard_window($x);

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 2, 'two nodes');
is($content[0]->{border}, 'none', 'no border');
is($content[1]->{border}, 'normal', 'normal border');
ok(!$content[0]->{focused}, 'first one not focused');

cmd qq|[con_mark="bleh"] focus|;

@content = @{get_ws_content($tmp)};
ok($content[0]->{focused}, 'first node focused');

exit_gracefully($process->pid);

##############################################################
# 4: multiple criteria for the for_window command
##############################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [class="borderless" title="usethis"] border none
EOT

$process = launch_with_config($config);

$tmp = fresh_workspace;

$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#00ff00',
);

$window->_create;

set_wm_class($window->id, 'borderless', 'borderless');
$window->name('usethis');
$window->map;
sleep 0.25;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'none', 'no border');

$window->unmap;
sleep 0.25;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 0, 'no nodes on this workspace now');

set_wm_class($window->id, 'borderless', 'borderless');
$window->name('notthis');
$window->map;
sleep 0.25;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'normal', 'no border');


exit_gracefully($process->pid);

done_testing;
