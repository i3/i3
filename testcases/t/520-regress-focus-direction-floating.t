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
# Ensure that `focus [direction]` will focus an existing floating con when no
# tiling con exists on the output in [direction] when focusing across outputs
# Bug still in: 4.7.2-204-g893dbae
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

workspace ws_left output fake-0
workspace ws_right output fake-1

mouse_warping none

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

cmd 'workspace ws_left';
my $win = open_window();

cmd 'floating enable';
cmd 'focus output right';
cmd 'focus left';

is($x->input_focus, $win->id,
    'Focusing across outputs with `focus [direction]` should focus an existing floating con when no tiling con exists on the output in [direction].');

done_testing;
