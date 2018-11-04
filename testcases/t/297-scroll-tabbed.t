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
# Tests if scrolling the tab bar on a tabbed container works and verifies that
# only one window is focused as a result.
# Ticket: #3215 (PR)
# Bug still in: 4.15-92-g666aa9e0
use i3test;
use i3test::XTEST;

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

done_testing;
