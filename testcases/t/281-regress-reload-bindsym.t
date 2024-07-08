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
# Test that the binding event works properly
# Ticket: #1210
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym r reload
EOT

SKIP: {
    qx(which xdotool 2> /dev/null);

    skip 'xdotool is required to test the binding event. `[apt-get install|pacman -S] xdotool`', 1 if $?;

    qx(xdotool key r);

    does_i3_live;

}
done_testing;
