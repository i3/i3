#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
# checks if i3 starts up on workspace '1' or the first configured named workspace
#
use X11::XCB qw(:all);
use X11::XCB::Connection;
use i3test;

my $x = X11::XCB::Connection->new;

##############################################################
# 1: i3 should start with workspace '1'
##############################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $process = launch_with_config($config);

my @names = @{get_workspace_names()};
cmp_deeply(\@names, [ '1' ], 'i3 starts on workspace 1 without any configuration');

exit_gracefully($process->pid);

##############################################################
# 2: with named workspaces, i3 should start on the first named one
##############################################################


my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym Mod1+1 workspace foobar
EOT

my $process = launch_with_config($config);

my @names = @{get_workspace_names()};
cmp_deeply(\@names, [ 'foobar' ], 'i3 starts on named workspace foobar');

exit_gracefully($process->pid);

done_testing;
