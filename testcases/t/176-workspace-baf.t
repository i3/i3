#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
# Checks if the 'workspace back_and_forth' command and the
# 'workspace_auto_back_and_forth' config directive work correctly.
#

use i3test;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $pid = launch_with_config($config);

my $first_ws = fresh_workspace;
ok(get_ws($first_ws)->{focused}, 'first workspace focused');

my $second_ws = fresh_workspace;
ok(get_ws($second_ws)->{focused}, 'second workspace focused');

my $third_ws = fresh_workspace;
ok(get_ws($third_ws)->{focused}, 'third workspace focused');

cmd 'workspace back_and_forth';
ok(get_ws($second_ws)->{focused}, 'second workspace focused');

#####################################################################
# test that without workspace_auto_back_and_forth switching to the same
# workspace that is currently focused is a no-op
#####################################################################

cmd qq|workspace "$second_ws"|;
ok(get_ws($second_ws)->{focused}, 'second workspace still focused');

exit_gracefully($pid);

#####################################################################
# the same test, but with workspace_auto_back_and_forth
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
workspace_auto_back_and_forth yes
EOT

$pid = launch_with_config($config);

$first_ws = fresh_workspace;
ok(get_ws($first_ws)->{focused}, 'first workspace focused');

$second_ws = fresh_workspace;
ok(get_ws($second_ws)->{focused}, 'second workspace focused');

$third_ws = fresh_workspace;
ok(get_ws($third_ws)->{focused}, 'third workspace focused');

cmd qq|workspace "$third_ws"|;
ok(get_ws($second_ws)->{focused}, 'second workspace focused');

exit_gracefully($pid);

done_testing;
