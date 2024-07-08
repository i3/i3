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
# Test that i3 doesn't crash if the binding command is empty.
# Ticket: #5000

use i3test i3_autostart => 0;

my $config = <<EOT;
bindsym X
EOT

my $pid = launch_with_config($config);
does_i3_live;

exit_gracefully($pid);
done_testing;
