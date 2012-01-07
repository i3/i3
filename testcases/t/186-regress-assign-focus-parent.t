#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression test: New windows were not opened in the correct place if they
# matched an assignment.
# Wrong behaviour manifested itself up to (including) commit
# f78caf8c5815ae7a66de9e4b734546fd740cc19d
#
use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

assign [title="testcase"] targetws
EOT

my $pid = launch_with_config($config);

my $i3 = i3(get_socket_path(0));

cmd 'workspace targetws';

open_window(name => "testcase");

my $nodes = get_ws_content('targetws');
is(scalar @$nodes, 1, 'precisely one window');

open_window(name => "testcase");

$nodes = get_ws_content('targetws');
is(scalar @$nodes, 2, 'precisely two windows');

cmd 'split v';

open_window(name => "testcase");

$nodes = get_ws_content('targetws');
is(scalar @$nodes, 2, 'still two windows');

# focus parent. the new window should now be opened right next to the last one.
cmd 'focus parent';

open_window(name => "testcase");

$nodes = get_ws_content('targetws');
is(scalar @$nodes, 3, 'new window opened next to last one');

exit_gracefully($pid);

done_testing;
