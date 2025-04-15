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
# Basic gaps functionality test
# Ticket: #3724

use i3test i3_autostart => 0;
use i3test::Util qw(slurp);

my $config = <<EOT;
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
    local $Test::Builder::Level = $Test::Builder::Level + 1;

    sync_with_i3;

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
is_gaps();

# Verify global gaps do influence all workspaces
cmd 'gaps outer all set 30px';

$outer_gaps = 30;
$total_gaps = $outer_gaps + $inner_gaps;
is_gaps();

# Verify negative outer gaps compensate inner gaps, resulting only in gaps
# in between adjacent windows or split containers, not towards the screen edges.
cmd 'gaps outer all set -10px';

sub is_gaps_in_between_only {
    local $Test::Builder::Level = $Test::Builder::Level + 1;

    sync_with_i3;

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
is_gaps_in_between_only();

exit_gracefully($pid);

################################################################################
# Ensure gaps configuration does not need to be ordered from least to most specific
################################################################################

$config = <<EOT;
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

$inner_gaps = 16;
$outer_gaps = 0;
$total_gaps = $outer_gaps + $inner_gaps;
is_gaps();

exit_gracefully($pid);

################################################################################
# Ensure stacked/tabbed containers are properly inset even when they are part
# of a splith/splitv container (issue #5261).
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

gaps inner 10

default_border pixel 0
EOT

$pid = launch_with_config($config);

fresh_workspace;

my $helper = open_window;
$right = open_window;
sync_with_i3;
cmd 'focus left';
cmd 'splitv';
$left = open_window;
sync_with_i3;
cmd 'splith';
cmd 'layout stacked';
sync_with_i3;
$helper->destroy;

$inner_gaps = 10;
$outer_gaps = 0;
$total_gaps = $outer_gaps + $inner_gaps;
is_gaps();

exit_gracefully($pid);

################################################################################
# Ensure floating split containers don’t get gaps (issue #5272).
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

gaps inner 10

default_border pixel 0
EOT

$pid = launch_with_config($config);

fresh_workspace;

my $floating = open_floating_window;
sync_with_i3;

my $orig_rect = $floating->rect;
cmd 'border pixel 0';
sync_with_i3;
is_deeply(scalar $floating->rect, $orig_rect, 'floating window position unchanged after border pixel 0');

cmd 'layout stacking';
sync_with_i3;
is_deeply(scalar $floating->rect, $orig_rect, 'floating window position unchanged after border pixel 0');

exit_gracefully($pid);

################################################################################
# Ensure existing workspaces pick up changes in gap assignments (issue #5257).
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

gaps inner 10

default_border pixel 0
EOT

$pid = launch_with_config($config);

cmd 'workspace 2';

$left = open_window;
$right = open_window;
is_gaps();

my $version = i3()->get_version()->recv;
open(my $configfh, '>', $version->{'loaded_config_file_name'});
say $configfh <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

# Increase gaps for (existing) workspace 2 to 16px
workspace 2 gaps inner 16
gaps inner 10

default_border pixel 0
EOT
close($configfh);

cmd 'reload';

$inner_gaps = 16;
$outer_gaps = 0;
$total_gaps = $outer_gaps + $inner_gaps;
is_gaps();

exit_gracefully($pid);

################################################################################
# Ensure removing gaps from workspace works (issue #5282).
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

gaps inner 33
gaps outer 22
workspace 1 gaps outer 0
workspace 1 gaps inner 0
workspace 2 gaps outer 10
workspace 2 gaps inner 0
workspace 3 gaps outer 0
workspace 3 gaps inner 10
workspace 4 gaps left 10
workspace 4 gaps top 20
workspace 4 gaps inner 0
workspace 4 gaps bottom 0
workspace 4 gaps right 0

default_border pixel 0
EOT

$pid = launch_with_config($config);

# Everything disabled
cmd 'workspace 1';
kill_all_windows;
$left = open_window;
$right = open_window;

$inner_gaps = 0;
$total_gaps = 0;
is_gaps();

# Inner disabled
cmd 'workspace 2';
$left = open_window;
$right = open_window;

$inner_gaps = 0;
$total_gaps = 10;
is_gaps();

# Outer disabled
cmd 'workspace 3';
$left = open_window;
$right = open_window;

$inner_gaps = 10;
$total_gaps = 10;
is_gaps();

# More complicated example
cmd 'workspace 4';
$left = open_window;
$right = open_window;
sync_with_i3;

my $left_rect = $left->rect;
my $right_rect = $right->rect;
is_deeply($left_rect, {
x => 10,
y => 20,
width => $screen_width/2 - 10/2,
height => $screen_height - 20,
}, 'left window position and size matches gaps expectations');

is_deeply($right_rect, {
x => $left_rect->x + $left_rect->width,
y => 20,
width => $screen_width/2 - 10/2,
height => $left_rect->height,
}, 'right window position and size matches gaps expectations');


exit_gracefully($pid);

done_testing;
