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
# checks if i3 starts up on workspace '1' or the first configured named workspace
#
use i3test i3_autostart => 0;

##############################################################
# 1: i3 should start with workspace '1'
##############################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $pid = launch_with_config($config);

my @names = @{get_workspace_names()};
is_deeply(\@names, [ '1' ], 'i3 starts on workspace 1 without any configuration');

exit_gracefully($pid);

##############################################################
# 2: with named workspaces, i3 should start on the first named one
##############################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym Mod1+1 workspace foobar
EOT

$pid = launch_with_config($config);

@names = @{get_workspace_names()};
is_deeply(\@names, [ 'foobar' ], 'i3 starts on named workspace foobar');

exit_gracefully($pid);

##############################################################
# 3: the same test as 2, but with a quoted workspace name
##############################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym Mod1+1 workspace "foobar"
EOT

$pid = launch_with_config($config);

@names = @{get_workspace_names()};
is_deeply(\@names, [ 'foobar' ], 'i3 starts on named workspace foobar');

exit_gracefully($pid);

################################################################################
# 4: now with whitespace in front of the workspace number
################################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym Mod1+1 workspace   3
EOT

$pid = launch_with_config($config);

@names = @{get_workspace_names()};
is_deeply(\@names, [ '3' ], 'i3 starts on workspace 3 without whitespace');

exit_gracefully($pid);

################################################################################
# 5: now with a binding that contains multiple commands
################################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym Mod1+1 workspace 3; exec foo
EOT

$pid = launch_with_config($config);

@names = @{get_workspace_names()};
is_deeply(\@names, [ '3' ], 'i3 starts on workspace 3 without ;exec foo');

exit_gracefully($pid);

done_testing;
