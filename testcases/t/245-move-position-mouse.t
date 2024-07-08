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
# Tests the 'move [window|container] [to] position mouse|cursor|pointer command.
# Ticket: #1696
use i3test i3_autostart => 0;
use POSIX qw(floor);

sub warp_pointer {
    my ($pos_x, $pos_y) = @_;
    $x->root->warp_pointer($pos_x, $pos_y);
    sync_with_i3;
}

my ($pid, $config, $ws, $rect, @cons);
my $root_rect = $x->root->rect;

##########################################################################

##########################################################################
# Given a floating container and the cursor far from any edges, when
# moving the container to the mouse, then the container is moved such
# that the cursor is centered inside it.
##########################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT
$pid = launch_with_config($config);

$ws = fresh_workspace;
open_floating_window;
warp_pointer(100, 100);

cmd 'move position mouse';

@cons = @{get_ws($ws)->{floating_nodes}};
$rect = $cons[0]->{rect};

is(floor($rect->{x} + $rect->{width} / 2), 100, "con is centered around cursor horizontally");
is(floor($rect->{y} + $rect->{height} / 2), 100, "con is centered around cursor vertically");

exit_gracefully($pid);

##########################################################################
# Given a floating container and the cursor is in the left upper edge
# of the output, when moving the container to the mouse, then the
# container is moved but adjusted to stay in-bounds.
##########################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT
$pid = launch_with_config($config);

$ws = fresh_workspace;
open_floating_window;
warp_pointer(5, 5);

cmd 'move position mouse';

@cons = @{get_ws($ws)->{floating_nodes}};
$rect = $cons[0]->{rect};

is($rect->{x}, 0, "con is on the left edge");
is($rect->{y}, 0, "con is on the upper edge");

exit_gracefully($pid);

##########################################################################
# Given a floating container and the cursor is in the left right lower
# edge of the output, when moving the container to the mouse, then the
# container is moved but adjusted to stay in-bounds.
##########################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT
$pid = launch_with_config($config);

$ws = fresh_workspace;
open_floating_window;
warp_pointer($root_rect->width - 5, $root_rect->height - 5);

cmd 'move position mouse';

@cons = @{get_ws($ws)->{floating_nodes}};
$rect = $cons[0]->{rect};

is($rect->{x} + $rect->{width}, $root_rect->width, "con is on the right edge");
is($rect->{y} + $rect->{height}, $root_rect->height, "con is on the lower edge");

exit_gracefully($pid);

##########################################################################
# Given a floating container and the cursor being close to the corner of
# an output, when the container is moved to the mouse, then the container
# is moved but adjusted to the output boundaries.
# This test verifies that boundaries of individual outputs are respected
# and not just boundaries of the entire X root screen.
##########################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
fake-outputs 500x500+0+0,500x500+500+0,500x500+0+500,500x500+500+500
EOT
$pid = launch_with_config($config);

$ws = fresh_workspace;
open_floating_window;
warp_pointer(495, 495);

cmd 'move position mouse';

@cons = @{get_ws($ws)->{floating_nodes}};
$rect = $cons[0]->{rect};

is($rect->{x} + $rect->{width}, 500, "con is on the right edge");
is($rect->{y} + $rect->{height}, 500, "con is on the lower edge");

exit_gracefully($pid);

##########################################################################

done_testing;
