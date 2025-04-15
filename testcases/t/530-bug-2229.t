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
# Ticket: #2229
# Bug still in: 4.11-262-geb631ce
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 400x400+0+0,400x400+400+0
workspace_auto_back_and_forth no
EOT

# Set it up such that workspace 3 is on the left output and
# workspace 4 is on the right output
cmd 'focus output fake-0';
open_window;
cmd 'workspace 3';
cmd 'focus output fake-1';
cmd 'workspace 4';
open_window;

cmd 'move workspace to output left';

ok(!workspace_exists('3'), 'workspace 3 has been closed');

done_testing;
