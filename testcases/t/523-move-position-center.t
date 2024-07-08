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
# Verifies that 'move position center' moves floating cons to the center of
# the appropriate output.
# Ticket: #1211
# Bug still in: 4.9.1-108-g037cb31
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0

workspace left output fake-0
workspace right output fake-1
EOT

#####################################################################
# Verify that 'move position center' on a floating window does not
# move it to another output.
#####################################################################

cmd 'workspace left';

# Sync in case focus switched outputs due to the workspace change.
sync_with_i3;

my $floating = open_floating_window;

# Center the window on the left workspace
cmd 'move position center';
sync_with_i3;

is(scalar @{get_ws('left')->{floating_nodes}}, 1, 'one floating node on left ws');
is(scalar @{get_ws('right')->{floating_nodes}}, 0, 'no floating nodes on right ws');

# Center the window on the right workspace
cmd 'move workspace right; workspace right; move position center';
sync_with_i3;

is(scalar @{get_ws('left')->{floating_nodes}}, 0, 'no floating nodes on left ws');
is(scalar @{get_ws('right')->{floating_nodes}}, 1, 'one floating node on right ws');

done_testing;
