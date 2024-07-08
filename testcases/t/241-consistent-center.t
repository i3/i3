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
# Verifies that most of i3's centering methods produce consistent results.
# Decorations are disabled to avoid floating_enable's logic which shifts
# windows upwards dependent on their decoration height.
#
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_window none
new_float none
EOT

#####################################################################
# Open a floating window, verifying that its initial position is
# centered, and also verify that both centering methods leave it in
# its original spot.
#####################################################################

my $first = open_floating_window;

my $initial = $first->rect;
is(int($initial->{x} + $initial->{width} / 2), int($x->root->rect->width / 2),
   'x coordinates match');
is(int($initial->{y} + $initial->{height} / 2), int($x->root->rect->height / 2),
   'y coordinates match');

cmd 'move position center';

my $new = $first->rect;
is($initial->{x}, $new->{x}, 'x coordinates match');
is($initial->{y}, $new->{y}, 'y coordinates match');

cmd 'move absolute position center';

$new = $first->rect;
is($initial->{x}, $new->{x}, 'x coordinates match');
is($initial->{y}, $new->{y}, 'y coordinates match');

#####################################################################
# Create a second window and move it into and out of the scratchpad.
# Because it hasn't been moved or resized, it should be floated in
# the center of the screen when pulled out of the scratchpad.
#####################################################################

my $second = open_window;

cmd 'move scratchpad, scratchpad show';

$new = $second->rect;
my $mid_init = $initial->{x} + int($initial->{width} / 2);
my $mid_new = $new->{x} + int($new->{width} / 2);
is($mid_init, $mid_new, 'x midpoint is ws center');

$mid_init = $initial->{y} + int($initial->{height} / 2);
$mid_new = $new->{y} + int($new->{height} / 2);
is($mid_init, $mid_new, 'y midpoint is ws center');

#####################################################################
# Verify that manually floating a tiled window results in proper
# centering.
#####################################################################

my $third = open_window;

cmd 'floating enable';

$new = $third->rect;
is($initial->{x}, $new->{x}, 'x coordinates match');
is($initial->{y}, $new->{y}, 'y coordinates match');

#####################################################################
# Create a child window of the previous window, which should result
# in the new window being centered over the last one.
#####################################################################

my $fourth = open_window( dont_map => 1, client_leader => $third );
$fourth->map;
sync_with_i3;

my $child = $fourth->rect;
is($new->{x}, $child->{x}, 'x coordinates match');
is($new->{y}, $child->{y}, 'y coordinates match');

done_testing;
