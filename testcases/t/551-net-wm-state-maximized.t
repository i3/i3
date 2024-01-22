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

sub maximized_vert {
    my ($window) = @_;
    return net_wm_state_contains($window, '_NET_WM_STATE_MAXIMIZED_VERT');
}

sub maximized_horz {
    my ($window) = @_;
    return net_wm_state_contains($window, '_NET_WM_STATE_MAXIMIZED_HORZ');
}

# Returns true if the given window is maximized in both directions.
sub maximized_both {
    my ($window) = @_;
    return maximized_vert($window) && maximized_horz($window);
}

# Returns true if the given window is maximized in neither direction.
sub maximized_neither {
    my ($window) = @_;
    return !maximized_vert($window) && !maximized_horz($window);
}

my ($winA, $winB, $winC);
fresh_workspace;

$winA = open_window;
ok(maximized_both($winA), 'if there is just one window, it is maximized');

cmd 'fullscreen enable';
ok(maximized_neither($winA), 'fullscreen windows are not maximized');

cmd 'fullscreen disable';
ok(maximized_both($winA), 'disabling fullscreen sets maximized to true again');

cmd 'floating enable';
ok(maximized_neither($winA), 'floating windows are not maximized');

cmd 'floating disable';
ok(maximized_both($winA), 'disabling floating sets maximized to true again');

# Open a second window.
$winB = open_window;

# Windows in stacked or tabbed containers are considered maximized.
cmd 'layout stacking';
ok(maximized_both($winA) && maximized_both($winB),
   'stacking layout maximizes all windows');

cmd 'layout tabbed';
ok(maximized_both($winA) && maximized_both($winB),
   'tabbed layout maximizes all windows');

# Arrange the two windows with a vertical split.
cmd 'layout splitv';
ok(!maximized_vert($winA) && !maximized_vert($winB),
   'vertical split means children are not maximized vertically');
ok(maximized_horz($winA) && maximized_horz($winB),
   'children may still be maximized horizontally in a vertical split');

# Arrange the two windows with a horizontal split.
cmd 'layout splith';
ok(maximized_vert($winA) && maximized_vert($winB),
   'children may still be maximized vertically in a horizontal split');
ok(!maximized_horz($winA) && !maximized_horz($winB),
   'horizontal split means children are not maximized horizontally');

# Add a vertical split within the horizontal split, and open a third window.
cmd 'split vertical';
$winC = open_window;
ok(maximized_vert($winA), 'winA still reaches from top to bottom');
ok(!maximized_vert($winB) && !maximized_vert($winC),
   'winB and winC are split vertically, so they are not maximized vertically');
ok(!maximized_horz($winA) && !maximized_horz($winB) && !maximized_horz($winC),
   'horizontal split means children are not maximized horizontally');

# Change the vertical split container to a tabbed container.
cmd 'layout tabbed';
ok(maximized_vert($winA) && maximized_vert($winB) && maximized_vert($winC),
   'all windows now reach from top to bottom');
ok(!maximized_horz($winA) && !maximized_horz($winB) && !maximized_horz($winC),
   'horizontal split means children are not maximized horizontally');

done_testing;
