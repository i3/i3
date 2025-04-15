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
# Verifies that internal workspaces (those whose name starts with __) cannot be
# used in all commands that deal with workspaces.
# Ticket: #1209
# Bug still in: 4.7.2-154-g144e3fb
use i3test i3_autostart => 0;

sub internal_workspaces {
    scalar grep { /^__/ } @{get_workspace_names()}
}

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $pid = launch_with_config($config);

is(internal_workspaces(), 0, 'No internal workspaces');

cmd 'workspace __foo';
is(internal_workspaces(), 0, 'No internal workspaces');

cmd 'move to workspace __foo';
is(internal_workspaces(), 0, 'No internal workspaces');

cmd 'rename workspace to __foo';
is(internal_workspaces(), 0, 'No internal workspaces');

exit_gracefully($pid);

################################################################################
# Verify that new workspace names starting with __ are ignored.
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym Mod1+1 workspace __foo
bindsym Mod1+2 workspace bar
EOT

$pid = launch_with_config($config);

is_deeply(get_workspace_names(), [ 'bar' ], 'New workspace called bar, not __foo');

exit_gracefully($pid);

done_testing;
