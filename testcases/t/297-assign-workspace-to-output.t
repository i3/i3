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
    is(get_output_for_workspace($workspace), $output, $msg);
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
done_testing;
