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
# Ensures floating windows don’t drop out of fullscreen mode when restarting
# and that they keep their geometry.
# Ticket: #1263
# Bug still in: 4.7.2-200-g570b572
use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

for_window [instance=__i3-test-window] floating enable, border pixel 1
EOT

my $pid = launch_with_config($config);
my $tmp = fresh_workspace;

my $window = open_window(wm_class => '__i3-test-window');

cmd 'fullscreen';

my ($nodes, $focus) = get_ws($tmp);
my $floating_win = $nodes->{floating_nodes}->[0]->{nodes}->[0];
is($floating_win->{fullscreen_mode}, 1, 'floating window in fullscreen mode');
my $old_geometry = $floating_win->{geometry};

cmd 'restart';

($nodes, $focus) = get_ws($tmp);
$floating_win = $nodes->{floating_nodes}->[0]->{nodes}->[0];
is($floating_win->{fullscreen_mode}, 1, 'floating window still in fullscreen mode');
is_deeply($floating_win->{geometry}, $old_geometry, 'floating window geometry still the same');

exit_gracefully($pid);

done_testing;
