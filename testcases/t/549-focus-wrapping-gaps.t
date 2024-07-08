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
# Ensure focus wrapping works with negative gaps
# Ticket: #5293
# Bug still in: 4.21-130-ged690c7b
use i3test i3_autostart => 0;

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

gaps inner 10
gaps outer -2

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

my $pid = launch_with_config($config);

cmd 'focus output fake-0, workspace left';
my $left = open_window;

cmd 'focus output fake-1, workspace right';
my $right = open_window;

is($x->input_focus, $right->id, 'right window focused');

cmd 'focus left';

is($x->input_focus, $left->id, 'left window focused');

exit_gracefully($pid);

done_testing;
