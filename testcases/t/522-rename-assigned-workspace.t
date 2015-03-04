#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
#
# Tests that workspaces are moved to the assigned output if they
# are renamed to an assigned name.
# Ticket: #1473

use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
fake-outputs 1024x768+0+0,1024x768+1024+0

workspace 1 output fake-0
workspace 2 output fake-1
workspace 3:foo output fake-1
workspace baz output fake-1
EOT

my $pid = launch_with_config($config);
my $i3 = i3(get_socket_path());
$i3->connect->recv;

# Returns the name of the output on which this workspace resides
sub get_output_for_workspace {
    my $ws_name = shift @_;

    foreach (grep { not $_->{name} =~ /^__/ } @{$i3->get_tree->recv->{nodes}}) {
        my $output = $_->{name};
        foreach (grep { $_->{name} =~ "content" } @{$_->{nodes}}) {
            return $output if $_->{nodes}[0]->{name} =~ $ws_name;
        }
    }
}

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


exit_gracefully($pid);
done_testing;
