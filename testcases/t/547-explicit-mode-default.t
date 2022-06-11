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
# Test that explicitly defined default mode doesn't cause segfault.
# Ticket: #5006
# Bug still in: 4.20-96-gce2665ca

use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
mode "default" {
    bindsym X resize
}
EOT

my $pid = launch_with_config($config);
does_i3_live;

exit_gracefully($pid);
done_testing;
