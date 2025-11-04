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
use i3test i3_autostart => 0;
use X11::XCB qw(PROP_MODE_REPLACE);

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [tiling] mark --add tiling
for_window [floating] mark --add floating

for_window [tiling_from="auto"] mark --add tiling_auto
for_window [floating_from="auto"] mark --add floating_auto

for_window [tiling_from="user"] mark --add tiling_user
for_window [floating_from="user"] mark --add floating_user
EOT

my $pid = launch_with_config($config);

##############################################################
# Check that the auto tiling / floating criteria work.
##############################################################

my $tmp = fresh_workspace;
my $A = open_window;
my $B = open_floating_window;

my @nodes = @{get_ws($tmp)->{nodes}};
cmp_ok(@nodes, '==', 1, 'one tiling container on this workspace');
is_deeply($nodes[0]->{marks}, [ 'tiling', 'tiling_auto' ], "mark set for 'tiling' criterion");

@nodes = @{get_ws($tmp)->{floating_nodes}};
cmp_ok(@nodes, '==', 1, 'one floating container on this workspace');
is_deeply($nodes[0]->{nodes}[0]->{marks}, [ 'floating', 'floating_auto' ], "mark set for 'floating' criterion");

################################################################################
# Check that the user tiling / floating criteria work.
# The following rules are triggered here: 'tiling', 'tiling_user', 'floating',
# 'floating_user'. Therefore, the old marks 'tiling' and 'floating' are
# replaced but the 'tiling_auto' and 'floating_auto' marks are preserved.
################################################################################

cmd '[id=' . $A->{id} . '] floating enable';
cmd '[id=' . $B->{id} . '] floating disable';

@nodes = @{get_ws($tmp)->{nodes}};
cmp_ok(@nodes, '==', 1, 'one tiling container on this workspace');
is_deeply($nodes[0]->{marks}, [ 'floating_auto', 'tiling', 'tiling_user' ], "Marks updated after 'floating_user' criterion");

@nodes = @{get_ws($tmp)->{floating_nodes}};
cmp_ok(@nodes, '==', 1, 'one floating container on this workspace');
is_deeply($nodes[0]->{nodes}[0]->{marks}, [ 'tiling_auto', 'floating', 'floating_user' ], "Marks updated after 'tiling_user' criterion");

################################################################################
# Check that the default and auto rules do not re-trigger
# Here, the windows are returned to their original state but since the rules
# `tiling`, `tiling_auto`, `floating` and `floating_auto` where already
# triggered, only the `tiling_user` and `floating_user` rules should trigger.
################################################################################

# Use 'mark' to clear old marks
cmd '[id=' . $A->{id} . '] mark A, floating disable';
cmd '[id=' . $B->{id} . '] mark B, floating enable';

@nodes = @{get_ws($tmp)->{nodes}};
cmp_ok(@nodes, '==', 1, 'one tiling container on this workspace');
is_deeply($nodes[0]->{marks}, [ 'A', 'tiling_user' ], "Only 'tiling_user' rule triggered");

@nodes = @{get_ws($tmp)->{floating_nodes}};
cmp_ok(@nodes, '==', 1, 'one floating container on this workspace');
is_deeply($nodes[0]->{nodes}[0]->{marks}, [ 'B', 'floating_user' ], "Only 'floating_user' rule triggered");

##############################################################

exit_gracefully($pid);

################################################################################
# Verify floating_from/tiling_from works as command criterion (issue #5258).
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

for_window [tiling] mark --add tiling
for_window [floating] mark --add floating
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;
$A = open_window;
$B = open_floating_window;

@nodes = @{get_ws($tmp)->{nodes}};
cmp_ok(@nodes, '==', 1, 'one tiling container on this workspace');
is_deeply($nodes[0]->{marks}, [ 'tiling' ], "mark set for 'tiling' criterion");

@nodes = @{get_ws($tmp)->{floating_nodes}};
cmp_ok(@nodes, '==', 1, 'one floating container on this workspace');
is_deeply($nodes[0]->{nodes}[0]->{marks}, [ 'floating' ], "mark set for 'floating' criterion");

cmd '[tiling_from="auto" con_mark="tiling"] mark --add tiling_auto';
cmd '[floating_from="auto" con_mark="floating"] mark --add floating_auto';

@nodes = @{get_ws($tmp)->{nodes}};
cmp_ok(@nodes, '==', 1, 'one tiling container on this workspace');
is_deeply($nodes[0]->{marks}, [ 'tiling', 'tiling_auto' ], "mark set for 'tiling' criterion");

@nodes = @{get_ws($tmp)->{floating_nodes}};
cmp_ok(@nodes, '==', 1, 'one floating container on this workspace');
is_deeply($nodes[0]->{nodes}[0]->{marks}, [ 'floating', 'floating_auto' ], "mark set for 'floating' criterion");

exit_gracefully($pid);

done_testing;
