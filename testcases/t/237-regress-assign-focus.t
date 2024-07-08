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
# Verifies that using layout tabbed followed by focus (on a window that is
# assigned to an invisible workspace) will not crash i3.
# Ticket: #1338
# Bug still in: 4.8-91-g294d52e
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

assign [title=".*"] 1
for_window [title=".*"] layout tabbed, focus
EOT

# Switch away from workspace 1
my $tmp = fresh_workspace;

my $win = open_window;

does_i3_live;

done_testing;
