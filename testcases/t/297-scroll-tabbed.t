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
# Tests if scrolling the tab bar on a tabbed container works and verifies that
# only one window is focused as a result.
# Ticket: #3215 (PR)
# Bug still in: 4.15-92-g666aa9e0
use i3test i3_autostart => 0;
use i3test::XTEST;

my $pid = launch_with_config('-default');

sub scroll_down {
    # button5 = scroll down
    xtest_button_press(5, 3, 3);
    xtest_button_release(5, 3, 3);
    xtest_sync_with_i3;
}

sub scroll_up {
    # button4 = scroll up
    xtest_button_press(4, 3, 3);
    xtest_button_release(4, 3, 3);
    xtest_sync_with_i3;
}

# Decoration of top left window.
$x->root->warp_pointer(3, 3);

# H [ T [ H [ A B ] C D V [ E F ] ] G ]
# Inner horizontal split.
open_window;
cmd 'layout tabbed';
cmd 'splith';
my $first = open_window;
cmd 'focus parent';
# Simple tabs.
open_window;
my $second_last = open_window;
# V-Split container
open_window;
cmd 'splitv';
my $last = open_window;
# Second child of the outer horizontal split, next to the tabbed one.
my $outside = open_window;
cmd 'move right, move right';

cmd '[id=' . $first->id . '] focus';

# Scroll from first to last.
scroll_down;
scroll_down;
is($x->input_focus, $second_last->id, 'Sanity check: scrolling');
scroll_down;
is($x->input_focus, $last->id, 'Last window focused through scrolling');
scroll_down;
is($x->input_focus, $last->id, 'Scrolling again doesn\'t leave the tabbed container and doesn\'t focus the whole sibling');

# Scroll from last to first.
scroll_up;
is($x->input_focus, $second_last->id, 'Scrolling up works');
scroll_up;
scroll_up;
is($x->input_focus, $first->id, 'First window focused through scrolling');
scroll_up;
is($x->input_focus, $first->id, 'Scrolling again doesn\'t focus the whole sibling');

# Try scrolling with another window focused
cmd '[id=' . $outside->id . '] focus';
scroll_up;
is($x->input_focus, $first->id, 'Scrolling from outside the tabbed container works');

exit_gracefully($pid);

###############################################################################
# Test that focus changes workspace correctly with 'focus_follows_mouse no'
# See issue #5472.
###############################################################################
$pid = launch_with_config(<<EOT
font font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
focus_follows_mouse no
fake-outputs 1024x768+0+0,1024x768+1024+0
EOT
);

my $ws1 = fresh_workspace(output => 0);
$first = open_window;
cmd 'layout tabbed';
is($x->input_focus, $first->id, 'sanity check: window focused');
open_window;

my $ws2 = fresh_workspace(output => 1);
ok(get_ws($ws2)->{focused}, 'sanity check: second workspace focused');

# Decoration of top left window.
$x->root->warp_pointer(3, 3);

my @events = events_for( sub { scroll_up }, 'workspace');
is($x->input_focus, $first->id, 'window focused');
is(scalar @events, 1, 'Received 1 workspace event');
is($events[0]->{change}, 'focus', 'Event has change = focus');
is($events[0]->{current}->{name}, $ws1, 'new == ws1');
is($events[0]->{old}->{name}, $ws2, 'old == ws2');

exit_gracefully($pid);

done_testing;
