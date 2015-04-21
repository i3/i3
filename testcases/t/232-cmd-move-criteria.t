#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Test that the `move [direction]` command works with criteria
# Bug still in: 4.8-16-g6888a1f
use i3test;

my ($ws, $win1, $win2, $win3, $ws_con);

###############################################################################
# Tets moving with 'id' criterion.
###############################################################################

$ws = fresh_workspace;

$win1 = open_window;
$win2 = open_window;
$win3 = open_window;

# move win1 from the left to the right
cmd '[id="' . $win1->{id} . '"] move right';

# now they should be switched, with win2 still being focused
$ws_con = get_ws($ws);

# win2 should be on the left
is($ws_con->{nodes}[0]->{window}, $win2->{id}, 'the `move [direction]` command should work with criteria');
is($x->input_focus, $win3->{id}, 'it should not disturb focus');

###############################################################################
# Tets moving with 'window_type' criterion.
###############################################################################

# test all window types
my %window_types = ( 
    'normal'        => '_NET_WM_WINDOW_TYPE_NORMAL',
    'dialog'        => '_NET_WM_WINDOW_TYPE_DIALOG',
    'utility'       => '_NET_WM_WINDOW_TYPE_UTILITY',
    'toolbar'       => '_NET_WM_WINDOW_TYPE_TOOLBAR',
    'splash'        => '_NET_WM_WINDOW_TYPE_SPLASH',
    'menu'          => '_NET_WM_WINDOW_TYPE_MENU',
    'dropdown_menu' => '_NET_WM_WINDOW_TYPE_DROPDOWN_MENU',
    'popup_menu'    => '_NET_WM_WINDOW_TYPE_POPUP_MENU',
    'tooltip'       => '_NET_WM_WINDOW_TYPE_TOOLTIP'
);

while (my ($window_type, $atom) = each %window_types) {

    $ws = fresh_workspace;

    $win1 = open_window(window_type => $x->atom(name => $atom));
    $win2 = open_window;
    $win3 = open_window;

    cmd '[window_type="' . $window_type . '"] move right';

    $ws_con = get_ws($ws);
    is($ws_con->{nodes}[0]->{window}, $win2->{id}, 'the `move [direction]` command should work with window_type = ' . $window_type);
    is($x->input_focus, $win3->{id}, 'it should not disturb focus');

}

###############################################################################

done_testing;
