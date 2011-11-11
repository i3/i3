#!perl
# vim:ts=4:sw=4:expandtab
#
# The command "move workspace prev; workspace prev" will lead to an error.
# This regression is present in 7f9b65f6a752e454c492447be4e21e2ee8faf8fd
use i3test;

my $i3 = i3(get_socket_path());

# Open one workspace to move the con to
my $old = fresh_workspace;
my $keep_open_con = open_empty_con($i3);

# Get a workspace and open a container
my $tmp = fresh_workspace;
my $con = open_empty_con($i3);

is(@{get_ws_content($tmp)}, 1, 'one container');
is(@{get_ws_content($old)}, 1, 'one container on old ws');

cmd 'move workspace prev; workspace prev';

is(@{get_ws_content($old)}, 2, 'container moved away');

done_testing;
