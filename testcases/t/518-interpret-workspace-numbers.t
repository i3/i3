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
# Tests that workspace assignment config directives for plain numbers will
# assign any workspace of that number to the specified output.
# Ticket: #1238
# Bug still in: 4.7.2-147-g3760a48
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

workspace 1:override output fake-0
workspace 2 output fake-0
workspace 1 output fake-1
workspace 2:override output fake-1
workspace 3 output fake-0
workspace 3:override output doesnotexist fake-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

################################################################################
# Workspace assignments with bare numbers should be interpreted as `workspace
# number` config directives. Any workspace beginning with that number should be
# assigned to the specified output.
################################################################################

cmd 'focus output fake-1';
cmd 'workspace "2:foo"';
is(get_output_for_workspace('2:foo'), 'fake-0',
    'Workspaces should be assigned by number when the assignment is a plain number')
    or diag 'Since workspace number 2 is assigned to fake-0, 2:foo should open on fake-0';

cmd 'focus output fake-0';
cmd 'workspace "2:override"';
is(get_output_for_workspace('2:override'), 'fake-1',
    'Workspace assignments by name should override numbered assignments')
    or diag 'Since workspace "2:override" is assigned by name to fake-1, it should open on fake-1';

cmd 'focus output fake-1';
cmd 'workspace "1:override"';
is(get_output_for_workspace('1:override'), 'fake-0',
    'Assignment rules should not be affected by the order assignments are declared')
    or diag 'Since workspace "1:override" is assigned by name to fake-0, it should open on fake-0';

cmd 'focus output fake-1';
cmd 'workspace "3:override"';
is(get_output_for_workspace('3:override'), 'fake-1',
   'Assignment rules should not be affected by multiple output assignments')
    or diag 'Since workspace "3:override" is assigned by name to fake-1, it should open on fake-1';

done_testing;
