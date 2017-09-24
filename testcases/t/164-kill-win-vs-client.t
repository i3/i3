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
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Tests if WM_STATE is WM_STATE_NORMAL when mapped and WM_STATE_WITHDRAWN when
# unmapped.
#
use i3test;

sub two_windows {
    my $tmp = fresh_workspace;

    ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

    my $first = open_window;
    my $second = open_window;

    is($x->input_focus, $second->id, 'second window focused');
    ok(@{get_ws_content($tmp)} == 2, 'two containers opened');

    return $tmp;
}

##############################################################
# 1: open two windows (in the same client), kill one and see if
# the other one is still there
##############################################################

my $tmp = two_windows;

cmd 'kill';
sync_with_i3;

ok(@{get_ws_content($tmp)} == 1, 'one container left after killing');

##############################################################
# 2: same test case as test 1, but with the explicit variant
# 'kill window'
##############################################################

$tmp = two_windows;

cmd 'kill window';
sync_with_i3;

ok(@{get_ws_content($tmp)} == 1, 'one container left after killing');

##############################################################
# 3: open two windows (in the same client), use 'kill client'
# and check if both are gone
##############################################################

$tmp = two_windows;

cmd 'kill client';
# We need to re-establish the X11 connection which we just killed :).
$x = i3test::X11->new;
sync_with_i3(no_cache => 1);

ok(@{get_ws_content($tmp)} == 0, 'no containers left after killing');

done_testing;
