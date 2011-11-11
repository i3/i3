#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests if empty workspaces are closed when the last child
# exits, as long as they're not empty.
#
use i3test;

my $i3 = i3(get_socket_path());

# Get a workspace and open a container
my $ws = fresh_workspace;
my $con = open_empty_con($i3);

# Go to a second workspace, kill the container
fresh_workspace;
cmd "[con_id=\"$con\"] kill";

# The first workspace should have been closed
ok(!workspace_exists($ws), 'workspace closed');

done_testing;
