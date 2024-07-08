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
# Verifies that when using multiple keyboard layouts at the same time, bindings
# without a specified XKB group will work in all XKB groups.
# Ticket: #2062
# Bug still in: 4.11-103-gc8d51b4
# Bug introduced with commit 0e5180cae9e9295678e3f053042b559e82cb8c98
use i3test
    i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym Print nop Print
bindsym Mod4+Return nop Mod4+Return
EOT
use i3test::XTEST;
use ExtUtils::PkgConfig;

SKIP: {
    skip "libxcb-xkb too old (need >= 1.11)", 1 unless
        ExtUtils::PkgConfig->atleast_version('xcb-xkb', '1.11');
    skip "setxkbmap not found", 1 if
        system(q|setxkbmap -print >/dev/null|) != 0;

system(q|setxkbmap us,ru -option grp:alt_shift_toggle|);

is(listen_for_binding(
    sub {
        xtest_key_press(107);
        xtest_key_release(107);
        xtest_sync_with_i3;
    },
    ),
   'Print',
   'triggered the "Print" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(133); # Super_L
        xtest_key_press(36); # Return
        xtest_key_release(36); # Return
        xtest_key_release(133); # Super_L
        xtest_sync_with_i3;
    },
    ),
   'Mod4+Return',
   'triggered the "Mod4+Return" keybinding');

# Switch keyboard group to russian.
set_xkb_group(1);

is(listen_for_binding(
    sub {
        xtest_key_press(107);
        xtest_key_release(107);
        xtest_sync_with_i3;
    },
    ),
   'Print',
   'triggered the "Print" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(133); # Super_L
        xtest_key_press(36); # Return
        xtest_key_release(36); # Return
        xtest_key_release(133); # Super_L
        xtest_sync_with_i3;
    },
    ),
   'Mod4+Return',
   'triggered the "Mod4+Return" keybinding');

# Disable the grp:alt_shift_toggle option, as we use Alt+Shift in other testcases.
system(q|setxkbmap us -option|);

}

done_testing;
