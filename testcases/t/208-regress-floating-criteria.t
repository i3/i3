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
# Regression test for focus handling when using floating enable/disable with
# criteria for windows on non-visible workspaces.
# Ticket: #1027
# Bug still in: 4.5.1-90-g6582da9
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

assign [class="^special$"] → mail
for_window [class="^special$"] floating enable, floating disable
EOT

my $window = open_window(
    name => 'Borderless window',
    wm_class => 'special',
    dont_map => 1,
);
$window->map;

sync_with_i3;

cmd '[class="^special$"] focus';

does_i3_live;

done_testing;
