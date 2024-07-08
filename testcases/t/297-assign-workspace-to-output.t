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
# Test assignments of workspaces to outputs.
use i3test i3_autostart => 0;

################################################################################
# Test initial workspaces.
################################################################################

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0,1024x768+1024+768,1024x768+0+768

bindsym Mod1+x workspace bindingname

workspace 9 output doesnotexist
workspace special output fake-0
workspace 1 output doesnotexist
workspace dontusethisname output doesnotexist
workspace donotoverride output fake-0
workspace 2 output fake-0
workspace 3 output fake-0
EOT

my $pid = launch_with_config($config);

sub check_output {
    my ($workspace, $output, $msg) = @_;
    is(get_output_for_workspace($workspace), $output, "[$workspace->$output] " . $msg);
}

check_output('9', '', 'Numbered workspace with a big number that is assigned to output that does not exist is not used');
check_output('special', 'fake-0', 'Output gets special workspace because of assignment');
check_output('bindingname', 'fake-1', 'Bindings give workspace names');
check_output('1', 'fake-2', 'Numbered workspace that is assigned to output that does not exist is used');
check_output('2', '', 'Numbered workspace assigned to output with existing workspace is not used');
check_output('3', '', 'Numbered workspace assigned to output with existing workspace is not used');
check_output('4', 'fake-3', 'First available number that is not assigned to existing output is used');
check_output('dontusethisname', '', 'Named workspace that is assigned to output that does not exist is not used');
check_output('donotoverride', '', 'Named workspace assigned to already occupied output is not used');

exit_gracefully($pid);

################################################################################
# Test multiple assignments
################################################################################

sub do_test {
    my ($workspace, $output, $msg) = @_;
    cmd 'focus output ' . ($output eq 'fake-3' ? 'fake-0' : 'fake-3');

    cmd "workspace $workspace";
    check_output($workspace, $output, $msg)
}

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0P,1024x768+1024+0,1024x768+1024+768,1024x768+0+768

workspace 1 output fake-0
workspace 1 output fake-1 fake-2
workspace 2 output fake-3 fake-4 fake-0 fake-1
workspace 3 output these outputs do not exist but these do: fake-0 fake-3
workspace 4 output whitespace                    fake-0
workspace foo output doesnotexist1 doesnotexist2 doesnotexist3
workspace bar output doesnotexist
workspace bar output fake-0
workspace 5 output fake-0
workspace 5:xxx output fake-1
workspace 6:xxx output fake-0
workspace 6 output fake-1
workspace 7 output nonprimary primary
workspace 8 output doesnotexist primary
EOT

$pid = launch_with_config($config);

do_test('1', 'fake-0', 'Multiple assignments do not override a single one');
do_test('2', 'fake-3', 'First output out of multiple assignments is used');
do_test('3', 'fake-0', 'First existing output is used');
do_test('4', 'fake-0', 'Excessive whitespace is ok');
do_test('5', 'fake-0', 'Numbered assignment ok');
do_test('5:xxx', 'fake-1', 'Named assignment overrides number');
do_test('6', 'fake-1', 'Numbered assignment ok');
do_test('6:xxx', 'fake-0', 'Named assignment overrides number');
do_test('7', 'fake-1', 'Non-primary output');
do_test('8', 'fake-0', 'Primary output');
do_test('9', 'fake-2', 'Numbered initialization for fake-2');

cmd 'focus output fake-0, workspace foo';
check_output('foo', 'fake-0', 'Workspace with only non-existing assigned outputs opened in current output');

cmd 'focus output fake-0, workspace bar';
check_output('bar', 'fake-0', 'Second workspace assignment line ignored');

# Moving assigned workspaces.
cmd 'workspace 2, move workspace to output left';
check_output('2', 'fake-2', 'Moved assigned workspace up');

# Specific name overrides assignment by number after renaming
# See #4021
cmd 'workspace 5, rename workspace to 5:xxx';
check_output('5:xxx', 'fake-1', 'workspace triggered correct, specific assignment after renaming');

# Same but opposite order
cmd 'workspace 6, rename workspace to 6:xxx';
check_output('6:xxx', 'fake-0', 'workspace triggered correct, specific assignment after renaming');

exit_gracefully($pid);
done_testing;
