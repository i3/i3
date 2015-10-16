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
# Tests whether 'workspace next' works correctly.
#
use List::Util qw(first);
use i3test i3_autostart => 0;

sub assert_next {
    my ($expected) = @_;

    cmd 'workspace next';
    # We need to sync after changing focus to a different output to wait for the
    # EnterNotify to be processed, otherwise it will be processed at some point
    # later in time and mess up our subsequent tests.
    sync_with_i3;

    is(focused_ws, $expected, "workspace $expected focused");
}


my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT
my $pid = launch_with_config($config);

################################################################################
# Setup workspaces so that they stay open (with an empty container).
# Have only named workspaces in the 1st output and numbered + named workspaces
# in the 2nd output.
################################################################################

sync_with_i3;
$x->root->warp_pointer(0, 0);
sync_with_i3;

cmd 'workspace A';
# ensure workspace A stays open
open_window;

cmd 'workspace B';
# ensure workspace B stays open
open_window;

cmd 'workspace D';
# ensure workspace D stays open
open_window;

cmd 'workspace E';
# ensure workspace E stays open
open_window;

cmd 'focus output right';

cmd 'workspace 1';
# ensure workspace 1 stays open
open_window;

cmd 'workspace 2';
# ensure workspace 2 stays open
open_window;

cmd 'workspace 3';
# ensure workspace 3 stays open
open_window;

cmd 'workspace 4';
# ensure workspace 4 stays open
open_window;

cmd 'workspace 5';
# ensure workspace 5 stays open
open_window;

cmd 'workspace C';
# ensure workspace C stays open
open_window;

cmd 'workspace F';
# ensure workspace F stays open
open_window;

cmd 'focus output right';

################################################################################
# Use workspace next and verify the correct order.
################################################################################

# The current order should be:
# output 1: A, B, D, E
# output 2: 1, 2, 3, 4, 5, C, F

cmd 'workspace A';
is(focused_ws, 'A', 'back on workspace A');

assert_next('B');
assert_next('D');
assert_next('E');
assert_next('C');
assert_next('F');
assert_next('1');
assert_next('2');
assert_next('3');
assert_next('4');
assert_next('5');
assert_next('A');
assert_next('B');

exit_gracefully($pid);

done_testing;
