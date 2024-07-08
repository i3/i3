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
#
# Tests that workspaces are moved to the assigned output if they
# are renamed to an assigned name.
# Ticket: #1473

use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
fake-outputs 1024x768+0+0,1024x768+1024+0

workspace 1 output fake-0
workspace 2 output fake-1
workspace 3:foo output fake-1
workspace baz output fake-1
workspace 5 output left
workspace 6 output doesnotexist fake-0
workspace 7 output fake-1 fake-0
EOT

##########################################################################
# Renaming the workspace to an unassigned name does not move the workspace
# (regression test)
##########################################################################

cmd 'focus output fake-0';
cmd 'rename workspace to unassigned';
is(get_output_for_workspace('unassigned'), 'fake-0',
    'Unassigned workspace should stay on its output when being renamed');

##########################################################################
# Renaming a workspace by number only triggers the assignment
##########################################################################

cmd 'focus output fake-0';
cmd 'rename workspace to 2';
is(get_output_for_workspace('2'), 'fake-1',
    'Renaming the workspace to a number should move it to the assigned output');

##########################################################################
# Renaming a workspace by number and name triggers the assignment
##########################################################################

cmd 'focus output fake-0';
cmd 'rename workspace to "2:foo"';
is(get_output_for_workspace('2:foo'), 'fake-1',
    'Renaming the workspace to a number and name should move it to the assigned output');

##########################################################################
# Renaming a workspace by name only triggers the assignment
##########################################################################

cmd 'focus output fake-0';
cmd 'rename workspace to baz';
is(get_output_for_workspace('baz'), 'fake-1',
    'Renaming the workspace to a number and name should move it to the assigned output');

##########################################################################
# Renaming a workspace so that it is assigned a directional output does
# not move the workspace or crash
##########################################################################

cmd 'focus output fake-0';
cmd 'workspace bar';
cmd 'rename workspace to 5';
is(get_output_for_workspace('5'), 'fake-0',
    'Renaming the workspace to a workspace assigned to a directional output should not move the workspace');

##########################################################################
# Renaming an unfocused workspace, triggering an assignment to the output
# which holds the currently focused empty workspace should result in the
# original workspace replacing the empty one.
# See issue #3228.
##########################################################################

cmd 'workspace baz';
cmd 'rename workspace 5 to 2';
is(get_output_for_workspace('2'), 'fake-1',
    'Renaming an unfocused workspace, triggering an assignment to the output which holds the currently focused empty workspace should result in the original workspace replacing the empty one');

##########################################################################
# Renaming an unfocused empty workspace, triggering an assignment to the
# output which holds the currently focused non-empty workspace should
# close the empty workspace and not crash i3.
# See issue #3248.
##########################################################################

cmd 'workspace 1';
cmd 'workspace 2';
open_window;
cmd 'rename workspace 1 to baz';
is(get_output_for_workspace('baz'), '',
    'Renaming an unfocused empty workspace, triggering an assignment to the output which holds the currently focused non-empty workspace should close the empty workspace and not crash i3');
kill_all_windows;

##########################################################################
# Renaming a workspace with multiple assignments, where the first output
# doesn't exist.
##########################################################################

cmd 'focus output fake-1';
cmd 'rename workspace to 6';
is(get_output_for_workspace('6'), 'fake-0',
   'Renaming the workspace while first target output doesn\'t exist moves it to the second assigned output');

##########################################################################
# Renaming a workspace with multiple assignments, where both outputs exist
# moves it to the first output.
##########################################################################

cmd 'focus output fake-0';
cmd 'rename workspace to 7';
is(get_output_for_workspace('7'), 'fake-1',
   'Renaming a workspace with multiple assignments, where both outputs exist moves it to the first output.');

done_testing;
