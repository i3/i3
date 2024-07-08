#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • https://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • https://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • https://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Tests whether 'workspace next' works correctly.
#
use List::Util qw(first);
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

sub assert_next {
    my ($expected) = @_;

    cmd 'workspace next';
    # We need to sync after changing focus to a different output to wait for the
    # EnterNotify to be processed, otherwise it will be processed at some point
    # later in time and mess up our subsequent tests.
    sync_with_i3;

    is(focused_ws, $expected, "workspace $expected focused");
}

sub assert_prev {
    my ($expected) = @_;

    cmd 'workspace prev';
    # We need to sync after changing focus to a different output to wait for the
    # EnterNotify to be processed, otherwise it will be processed at some point
    # later in time and mess up our subsequent tests.
    sync_with_i3;

    is(focused_ws, $expected, "workspace $expected focused");
}

sync_with_i3;
$x->root->warp_pointer(0, 0);
sync_with_i3;

################################################################################
# Setup workspaces so that they stay open (with an empty container).
# open_window ensures, this
#
#                   numbered        numbered w/ names  named
# output 1 (left) : 1, 2, 3, 6, 7,  8:a, 8:b, 8:c      B, F, C
# output 2 (right): 4, 5,           8:d, 8:e,          A, D, E
#
################################################################################

cmd 'focus output right';
cmd 'workspace A'; open_window;
cmd 'workspace D'; open_window;
cmd 'workspace 4'; open_window;
cmd 'workspace 5'; open_window;
cmd 'workspace E'; open_window;
cmd 'workspace 8:d'; open_window;
cmd 'workspace 8:e'; open_window;

cmd 'focus output left';
cmd 'workspace 1'; open_window;
cmd 'workspace 2'; open_window;
cmd 'workspace B'; open_window;
cmd 'workspace 3'; open_window;
cmd 'workspace F'; open_window;
cmd 'workspace 6'; open_window;
cmd 'workspace C'; open_window;
cmd 'workspace 7'; open_window;
cmd 'workspace 8:a'; open_window;
cmd 'workspace 8:b'; open_window;
cmd 'workspace 8:c'; open_window;

################################################################################
# Use workspace next and verify the correct order.
# numbered -> numerical sort
# numbered w/ names -> numerical sort. Workspaces with the same number but
#     different names sort by output, followed by creation time on each output.
# named -> sort by creation time
################################################################################
cmd 'workspace 1';
is(focused_ws, '1', 'back on workspace 1');

assert_next('2');
assert_next('3');
assert_next('4');
assert_next('5');
assert_next('6');
assert_next('7');

assert_next('8:a');
assert_next('8:b');
assert_next('8:c');
assert_next('8:d');
assert_next('8:e');

assert_next('B');
assert_next('F');
assert_next('C');
assert_next('A');
assert_next('D');
assert_next('E');
assert_next('1');

cmd 'workspace 1';
is(focused_ws, '1', 'back on workspace 1');

assert_prev('E');
assert_prev('D');
assert_prev('A');
assert_prev('C');
assert_prev('F');
assert_prev('B');

assert_prev('8:e');
assert_prev('8:d');
assert_prev('8:c');
assert_prev('8:b');
assert_prev('8:a');

assert_prev('7');
assert_prev('6');
assert_prev('5');
assert_prev('4');
assert_prev('3');
assert_prev('2');
assert_prev('1');


done_testing;
