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

subtest 'two windows in default layout', sub {
    $winB = open_window;
    ok(maximized_vert($winA), 'vertically maximized');
    ok(maximized_vert($winB), 'vertically maximized');
    ok(!maximized_horz($winA), 'not horizontally maximized');
    ok(!maximized_horz($winB), 'not horizontally maximized');
    cmd 'kill';
};

cmd 'fullscreen enable';
ok(maximized_both($winA), 'fullscreen windows are maximized');

cmd 'fullscreen disable';
ok(maximized_both($winA), 'disabling fullscreen retains maximized');

cmd 'floating enable';
ok(maximized_neither($winA), 'floating windows are not maximized');

cmd 'floating disable';
ok(maximized_both($winA), 'disabling floating sets maximized to true again');

# Open a second window.
$winB = open_window;

# Windows in stacked or tabbed containers are considered maximized.
subtest 'stacking layout', sub {
    cmd 'layout stacking';
    ok(maximized_both($winA), 'A maximized');
    ok(maximized_both($winB), 'B maximized');
};

subtest 'tabbed layout', sub {
    cmd 'layout tabbed';
    ok(maximized_both($winA), 'A maximized');
    ok(maximized_both($winB), 'B maximized');
};

# Arrange the two windows with a vertical split.
subtest 'vertical split', sub {
    cmd 'layout splitv';
    ok(!maximized_vert($winA), 'A not maximized vertically');
    ok(!maximized_vert($winB), 'B not maximized vertically');
    ok(maximized_horz($winA), 'A maximized horizontally');
    ok(maximized_horz($winB), 'B maximized horizontally');
};

# Arrange the two windows with a horizontal split.
subtest 'horizontal split', sub {
    cmd 'layout splith';
    ok(maximized_vert($winA), 'A maximized vertically');
    ok(maximized_vert($winB), 'B maximized vertically');
    ok(!maximized_horz($winA), 'A not maximized horizontally');
    ok(!maximized_horz($winB), 'B not maximized horizontally');
};

# Add a vertical split within the horizontal split, and open a third window.
subtest 'vertical split within the horizontal split', sub {
    cmd 'split vertical';
    $winC = open_window;
    ok(maximized_vert($winA), 'maximized vertically');
    ok(!maximized_vert($winB), 'B not maximized vertically');
    ok(!maximized_vert($winC), 'C not maximized vertically');
    ok(!maximized_horz($winA), 'A not maximized horizontally');
    ok(!maximized_horz($winB), 'B not maximized horizontally');
    ok(!maximized_horz($winC), 'C not maximized horizontally');
};

# Change the vertical split container to a tabbed container.
subtest 'tabbed container within horizontal split', sub {
    cmd 'layout tabbed';
    ok(maximized_vert($winA), 'A maximized vertically');
    ok(maximized_vert($winB), 'B maximized vertically');
    ok(maximized_vert($winC), 'C maximized vertically');
    ok(!maximized_horz($winA), 'A not maximized horizontally');
    ok(!maximized_horz($winB), 'B not maximized horizontally');
    ok(!maximized_horz($winC), 'C not maximized horizontally');
};

done_testing;
