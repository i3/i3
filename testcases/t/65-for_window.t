#!perl
# vim:ts=4:sw=4:expandtab
#
#
use X11::XCB qw(:all);
use X11::XCB::Connection;
use i3test;

my $x = X11::XCB::Connection->new;

my $tmp = fresh_workspace;

##############################################################
# 1: test the following directive:
#    for_window [class="borderless"] border none
# by first creating a window with a different class (should get
# the normal border), then creating a window with the class
# "borderless" (should get no border)
##############################################################

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
        $window->id,
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

##############################################################
# 2: match on the title, check if for_window is really executed
# only once
##############################################################

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

done_testing;
