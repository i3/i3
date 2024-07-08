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
# Tests that the hide_edge_borders smart option works
# Ticket: #2188

use i3test i3_autostart => 0;

####################################################################
# 1: check that the borders are present on a floating windows
#####################################################################

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_window pixel 2
new_float pixel 2
hide_edge_borders smart
EOT

my $pid = launch_with_config($config);

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $floatwindow = open_floating_window;

my $wscontent = get_ws($tmp);

my @floating = @{$wscontent->{floating_nodes}};
ok(@floating == 1, 'one floating container opened');
is($floating[0]->{nodes}[0]->{current_border_width}, 2, 'floating current border width set to 2');
is($floatwindow->rect->width, $floating[0]->{rect}->{width} - 2*2, 'floating border width 2');

exit_gracefully($pid);

#####################################################################
# 2: check that the borders are present on a workspace with two tiled
# windows visible
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_window pixel 2
new_float pixel 2
hide_edge_borders smart
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $tilewindow = open_window;
my $tilewindow2 = open_window;

$wscontent = get_ws($tmp);

my @tiled = @{$wscontent->{nodes}};
ok(@tiled == 2, 'two tiled container opened');
is($tiled[0]->{current_border_width}, 2, 'first tiled current border width set to 2');
is($tilewindow->rect->width, $tiled[0]->{rect}->{width} - 2*2, 'first tiled border width 2');
is($tiled[1]->{current_border_width}, 2, 'second tiled current border width set to 2');
is($tilewindow2->rect->width, $tiled[1]->{rect}->{width} - 2*2, 'second tiled border width 2');

exit_gracefully($pid);

#####################################################################
# 3: check that the borders are hidden on a workspace with one tiled
# window visible
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_window pixel 2
new_float pixel 2
hide_edge_borders smart
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$tilewindow = open_window;

$wscontent = get_ws($tmp);

@tiled = @{$wscontent->{nodes}};
ok(@tiled == 1, 'one tiled container opened');
is($tiled[0]->{current_border_width}, 2, 'tiled current border width set to 2');
is($tilewindow->rect->width, $tiled[0]->{rect}->{width} - 2*0, 'single tiled border width 0');

exit_gracefully($pid);

#####################################################################
# 4: check that the borders are present on a workspace with two tiled
# windows visible, recursively
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_window pixel 2
new_float pixel 2
hide_edge_borders smart
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$tilewindow = open_window;
$tilewindow2 = open_window;
ok(@{get_ws_content($tmp)} == 2, 'two containers opened');

cmd 'layout tabbed';
ok(@{get_ws_content($tmp)} == 1, 'layout tabbed -> back to one container');

cmd 'focus parent';
my $tilewindow3 = open_window;
ok(@{get_ws_content($tmp)} == 2, 'after split & new window, two containers');

$wscontent = get_ws($tmp);

# Ensure i3’s X11 requests are processed before our inquiry via
# $tilewindow->rect:
sync_with_i3;

@tiled = @{$wscontent->{nodes}};
ok(@tiled == 2, 'two tiled container opened in another container');
is($tiled[0]->{current_border_width}, -1, 'first tiled current border width set to -1');
is($tilewindow->rect->width, $tiled[0]->{rect}->{width} - 2*2, 'first tiled border width 2');
is($tiled[1]->{current_border_width}, 2, 'second tiled current border width set to 2');
is($tilewindow2->rect->width, $tiled[1]->{rect}->{width} - 2*2, 'second tiled border width 2');

exit_gracefully($pid);

#####################################################################
# 5: check that the borders are visible on a workspace with one tiled
# window and edge gaps
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_window pixel 2
new_float pixel 2
gaps outer 5
hide_edge_borders smart_no_gaps
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$tilewindow = open_window;

$wscontent = get_ws($tmp);

@tiled = @{$wscontent->{nodes}};
ok(@tiled == 1, 'one tiled container opened');
is($tiled[0]->{current_border_width}, 2, 'tiled current border width set to 2');
is($tilewindow->rect->width, $tiled[0]->{rect}->{width} - 2*2, 'single tiled border width 2');

exit_gracefully($pid);

#####################################################################
# 5: check that the borders are hidden on a workspace with one tiled
# window with no gaps
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_window pixel 2
new_float pixel 2
hide_edge_borders smart_no_gaps
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$tilewindow = open_window;

$wscontent = get_ws($tmp);

@tiled = @{$wscontent->{nodes}};
ok(@tiled == 1, 'one tiled container opened');
is($tiled[0]->{current_border_width}, 2, 'tiled current border width set to 2');
is($tilewindow->rect->width, $tiled[0]->{rect}->{width} - 2*0, 'single tiled border width 0');

exit_gracefully($pid);



done_testing;
