#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests all kinds of matching methods
#
use i3test;
use X11::XCB qw(:all);

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

# Open a new window
my $x = X11::XCB::Connection->new;
my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#C0C0C0',
);

$window->map;
# give it some time to be picked up by the window manager
# TODO: better check for $window->mapped or something like that?
# maybe we can even wait for getting mapped?
my $c = 0;
while (@{get_ws_content($tmp)} == 0 and $c++ < 5) {
    sleep 0.25;
}
my $content = get_ws_content($tmp);
ok(@{$content} == 1, 'window mapped');
my $win = $content->[0];

######################################################################
# first test that matches which should not match this window really do
# not match it
######################################################################
# TODO: specify more match types
# we can match on any (non-empty) class here since that window does not have
# WM_CLASS set
cmd q|[class=".*"] kill|;
cmd q|[con_id="99999"] kill|;

$content = get_ws_content($tmp);
ok(@{$content} == 1, 'window still there');

# now kill the window
cmd 'nop now killing the window';
my $id = $win->{id};
cmd qq|[con_id="$id"] kill|;

# give i3 some time to pick up the UnmapNotify event
sleep 0.25;

cmd 'nop checking if its gone';
$content = get_ws_content($tmp);
ok(@{$content} == 0, 'window killed');

# TODO: same test, but with pcre expressions

######################################################################
# check that multiple criteria work are checked with a logical AND,
# not a logical OR (that is, matching is not cancelled after the first
# criterion matches).
######################################################################

$tmp = fresh_workspace;

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

my $left = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
);

$left->_create;
set_wm_class($left->id, 'special', 'special');
$left->name('left');
$left->map;
sleep 0.25;

my $right = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
);

$right->_create;
set_wm_class($right->id, 'special', 'special');
$right->name('right');
$right->map;
sleep 0.25;

# two windows should be here
$content = get_ws_content($tmp);
ok(@{$content} == 2, 'two windows opened');

cmd '[class="special" title="left"] kill';

sleep 0.25;

$content = get_ws_content($tmp);
is(@{$content}, 1, 'one window still there');

######################################################################
# check that regular expressions work
######################################################################

$tmp = fresh_workspace;

$left = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
);

$left->_create;
set_wm_class($left->id, 'special7', 'special7');
$left->name('left');
$left->map;
sleep 0.25;

# two windows should be here
$content = get_ws_content($tmp);
ok(@{$content} == 1, 'window opened');

cmd '[class="^special[0-9]$"] kill';

sleep 0.25;

$content = get_ws_content($tmp);
is(@{$content}, 0, 'window killed');

######################################################################
# check that UTF-8 works when matching
######################################################################

$tmp = fresh_workspace;

$left = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30 ],
    background_color => '#0000ff',
);

$left->_create;
set_wm_class($left->id, 'special7', 'special7');
$left->name('ä 3');
$left->map;
sleep 0.25;

# two windows should be here
$content = get_ws_content($tmp);
ok(@{$content} == 1, 'window opened');

cmd '[title="^\w [3]$"] kill';

sleep 0.25;

$content = get_ws_content($tmp);
is(@{$content}, 0, 'window killed');

done_testing;
