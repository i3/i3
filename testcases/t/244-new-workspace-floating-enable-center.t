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
# Ensures that 'move workspace $new, floating enable' on a marked window
# leaves the window centered on the new workspace.
# Bug still in: 4.10.2-137-ga4f0ed6
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_window none
new_float none
EOT

#####################################################################
# Open a tiled window, and then simultaneously move it to another
# workspace and float it, ensuring that it ends up centered.
#####################################################################

my $window = open_window;
my $unused = get_unused_workspace();

cmd "mark foo; [con_mark=\"foo\"] move workspace $unused, floating enable";

sync_with_i3;

my $pos = $window->rect;

is(int($pos->{x} + $pos->{width} / 2), int($x->root->rect->width / 2),
    'x coordinates match');
is(int($pos->{y} + $pos->{height} / 2), int($x->root->rect->height / 2),
    'y coordinates match');

done_testing;
