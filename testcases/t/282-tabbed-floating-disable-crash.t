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
# Verifies that i3 does not crash when floating and then unfloating an
# unfocused window within a tabbed container.
# Ticket: #1484
# Bug still in: 4.9.1-124-g856e1f9
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
workspace_layout tabbed
EOT

open_window;
open_window;

# Mark the second window, then focus the workspace.
cmd 'mark foo, focus parent, focus parent';

# Float and unfloat the marked window (without it being focused).
cmd '[con_mark=foo] floating enable, floating disable';

does_i3_live;

done_testing;
