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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Tests for setting and removing the _NET_WM_STATE_MAXIMIZED_VERT and
# _NET_WM_STATE_MAXIMIZED_HORZ atoms.
use i3test;
use X11::XCB qw(:all);

# Returns true if the given window is maximized in both directions.
sub maximized_both {
    my ($window) = @_;
    return net_wm_state_contains($window, '_NET_WM_STATE_MAXIMIZED_VERT') &&
           net_wm_state_contains($window, '_NET_WM_STATE_MAXIMIZED_HORZ');
}

# Returns true if the given window is maximized in neither direction.
sub maximized_neither {
    my ($window) = @_;
    return !net_wm_state_contains($window, '_NET_WM_STATE_MAXIMIZED_VERT') &&
           !net_wm_state_contains($window, '_NET_WM_STATE_MAXIMIZED_HORZ');
}

my $windowA;
fresh_workspace;
$windowA = open_window;
ok(maximized_both($windowA), 'if there is just one window, it is maximized');

cmd 'fullscreen enable';
ok(maximized_neither($windowA), 'fullscreen windows are not maximized');

cmd 'fullscreen disable';
ok(maximized_both($windowA), 'disabling fullscreen sets maximized to true again');

cmd 'floating enable';
ok(maximized_neither($windowA), 'floating windows are not maximized');

cmd 'floating disable';
ok(maximized_both($windowA), 'disabling floating sets maximized to true again');

# TODO: more coverage

done_testing;
