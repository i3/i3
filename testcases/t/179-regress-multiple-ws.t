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

is_num_children($tmp, 1, 'one container');
is_num_children($old, 1, 'one container on old ws');

cmd 'move workspace prev; workspace prev';

is_num_children($old, 2, 'container moved away');

done_testing;
