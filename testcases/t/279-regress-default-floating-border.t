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
# This is a regression test for a bug where a normal floating default border is
# not applied when the default tiling border is set to a pixel value.
# Ticket: #1305
# Bug still in: 4.8-62-g7381b50
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_window pixel 5
new_float normal
EOT

my $ws = fresh_workspace;

my $float_window = open_floating_window;

my @floating = @{get_ws($ws)->{floating_nodes}};

is($floating[0]->{nodes}[0]->{border}, 'normal', 'default floating border is `normal`');

done_testing;
