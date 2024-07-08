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
# Verifies that moving containers wraps across outputs.
# E.g. when you have a container on the right output and you move it to the
# right, it should appear on the left output.
# Bug still in: 4.4-106-g3cd4b8c
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

my $right = fresh_workspace(output => 1);
my $left = fresh_workspace(output => 0);

my $win = open_window;

is_num_children($left, 1, 'one container on left workspace');

cmd 'move container to output right';
cmd 'focus output right';

is_num_children($left, 0, 'no containers on left workspace');
is_num_children($right, 1, 'one container on right workspace');

cmd 'move container to output right';

is_num_children($left, 1, 'one container on left workspace');
is_num_children($right, 0, 'no containers on right workspace');

done_testing;
