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
# Basic gaps functionality test
# Ticket: #3724

use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

gaps inner 10
gaps outer 20

default_border pixel 0
EOT

my $tmp = fresh_workspace;

my $left = open_window;
my $right = open_window;

my $screen_width = 1280;
my $screen_height = 800;
my $outer_gaps = 20;
my $inner_gaps = 10;
my $total_gaps = $outer_gaps + $inner_gaps;
my $left_rect = $left->rect;
my $right_rect = $right->rect;

# Gaps toward one screen edge, each window covers half of the screen,
# each gets half of the inner gaps.
my $expected_width = ($screen_width / 2) - $total_gaps - ($inner_gaps / 2);

# Gaps toward the screen edges at top and bottom.
my $expected_height = $screen_height - 2 * $total_gaps;

is_deeply($left_rect, {
    x => $total_gaps,
    y => $total_gaps,
    width => $expected_width,
    height => $expected_height,
}, 'left window position and size matches gaps expectations');

is_deeply($right_rect, {
    x => $left_rect->x + $left_rect->width + $inner_gaps,
    y => $total_gaps,
    width => $expected_width,
    height => $expected_height,
}, 'right window position and size matches gaps expectations');

done_testing;
