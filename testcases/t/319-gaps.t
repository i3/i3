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

use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

gaps inner 10
gaps outer 20px

default_border pixel 0
EOT

my $pid = launch_with_config($config);

my $tmp = fresh_workspace;

my $left = open_window;
my $right = open_window;

my $screen_width = 1280;
my $screen_height = 800;
my $outer_gaps = 20;
my $inner_gaps = 10;
my $total_gaps = $outer_gaps + $inner_gaps;

sub is_gaps {
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
}

is_gaps();

################################################################################
# gaps command
################################################################################

# Verify gaps on a different workspace do not influence the existing workspace
fresh_workspace;
cmd 'gaps outer current set 30px';
cmd "workspace $tmp";
sync_with_i3;
is_gaps();

# Verify global gaps do influence all workspaces
cmd 'gaps outer all set 30px';
sync_with_i3;

$outer_gaps = 30;
$total_gaps = $outer_gaps + $inner_gaps;
is_gaps();

# Verify negative outer gaps compensate inner gaps, resulting only in gaps
# in between adjacent windows or split containers, not towards the screen edges.
cmd 'gaps outer all set -10px';
sync_with_i3;

sub is_gaps_in_between_only {
    my $left_rect = $left->rect;
    my $right_rect = $right->rect;

    # No gaps towards the screen edges, each window covers half of the screen,
    # each gets half of the inner gaps.
    my $expected_width = ($screen_width / 2) - ($inner_gaps / 2);

    # No gaps towards the screen edges at top and bottom.
    my $expected_height = $screen_height;

    is_deeply($left_rect, {
	x => 0,
	y => 0,
	width => $expected_width,
	height => $expected_height,
    }, 'left window position and size matches gaps expectations');

    is_deeply($right_rect, {
	x => $left_rect->x + $left_rect->width + $inner_gaps,
	y => 0,
	width => $expected_width,
	height => $expected_height,
    }, 'right window position and size matches gaps expectations');
}

is_gaps_in_between_only();

# Reduce the inner gaps and verify the outer gaps are adjusted to not
# over-compensate.
cmd 'gaps inner all set 6px';
$inner_gaps = 6;
$total_gaps = $outer_gaps + $inner_gaps;
sync_with_i3;
is_gaps_in_between_only();

exit_gracefully($pid);

################################################################################
# Ensure gaps configuration does not need to be ordered from least to most specific
################################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

# This should result in a gap of 16px, not 26px
workspace 2 gaps inner 16
gaps inner 10

default_border pixel 0
EOT

$pid = launch_with_config($config);

cmd 'workspace 2';

$left = open_window;
$right = open_window;
sync_with_i3;

$inner_gaps = 16;
$outer_gaps = 0;
$total_gaps = $outer_gaps + $inner_gaps;

is_gaps();

exit_gracefully($pid);

done_testing;
