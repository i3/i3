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
# Verifies that mouse bindings work on the root window if
# --whole-window is set.
# Ticket: #2115
use i3test i3_autostart => 0;
use i3test::XTEST;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

workspace_auto_back_and_forth no
bindsym --whole-window button4 workspace special
EOT

my $pid = launch_with_config($config);
fresh_workspace;

xtest_button_press(4, 50, 50);
xtest_button_release(4, 50, 50);
sync_with_i3;

is(focused_ws(), 'special', 'the binding was triggered');

exit_gracefully($pid);

done_testing;
