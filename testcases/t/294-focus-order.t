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
# Verify that the corrent focus stack order is preserved after various
# operations.
use i3test;

sub kill_and_confirm_focus {
    my $focus = shift;
    my $msg = shift;
    cmd "kill";
    sync_with_i3;
    is($x->input_focus, $focus, $msg);
}

my @windows;

sub focus_windows {
    for (my $i = $#windows; $i >= 0; $i--) {
        cmd '[id=' . $windows[$i]->id . '] focus';
    }
}

sub confirm_focus {
    my $msg = shift;
    sync_with_i3;
    is($x->input_focus, $windows[0]->id, $msg . ': window 0 focused');
    foreach my $i (1 .. $#windows) {
        kill_and_confirm_focus($windows[$i]->id, "$msg: window $i focused");
    }
    cmd 'kill';
    @windows = ();
}

#####################################################################
# Open 5 windows, focus them in a custom order and then change to
# tabbed layout. The focus order should be preserved.
#####################################################################

fresh_workspace;

$windows[3] = open_window;
$windows[1] = open_window;
$windows[0] = open_window;
$windows[2] = open_window;
$windows[4] = open_window;
focus_windows;

cmd 'layout tabbed';
confirm_focus('tabbed');

#####################################################################
# Same as above but with stacked.
#####################################################################

fresh_workspace;

$windows[3] = open_window;
$windows[1] = open_window;
$windows[0] = open_window;
$windows[2] = open_window;
$windows[4] = open_window;
focus_windows;

cmd 'layout stacked';
confirm_focus('stacked');

#####################################################################
# Open 4 windows horizontally, move the last one down. The focus
# order should be preserved.
#####################################################################

fresh_workspace;
$windows[3] = open_window;
$windows[2] = open_window;
$windows[1] = open_window;
$windows[0] = open_window;

cmd 'move down';
confirm_focus('split-h + move');

#####################################################################
# Same as above but with a vertical split.
#####################################################################

fresh_workspace;
$windows[3] = open_window;
cmd 'split v';
$windows[2] = open_window;
$windows[1] = open_window;
$windows[0] = open_window;

cmd 'move left';
confirm_focus('split-v + move');

done_testing;
