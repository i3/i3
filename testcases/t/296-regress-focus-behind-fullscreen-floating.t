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
# Test that directional focus gives focus to floating fullscreen containers when
# switching workspaces.
# Ticket: #3201
# Bug still in: 4.15-59-gb849fe3e
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

fresh_workspace(output => 0);
my $ws = fresh_workspace(output => 1);
open_window;
open_floating_window;
cmd 'fullscreen enable';
my $expected_focus = get_focused($ws);

cmd 'focus left';
cmd 'focus right';

is (get_focused($ws), $expected_focus, 'floating fullscreen window focused after directional focus');

done_testing;
