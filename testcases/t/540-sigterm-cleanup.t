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
# Tests that the socket file is cleaned up properly after gracefully
# shutting down i3 via SIGTERM.
# Ticket: #3049
use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $pid = launch_with_config($config, dont_add_socket_path => 1);
my $socket = get_socket_path();
ok(-S $socket, "socket $socket exists");

exit_forcefully($pid, 'TERM');

ok(!-e $socket, "socket $socket no longer exists");

done_testing;
