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
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Verifies that --release key bindings are not shadowed by non-release key
# bindings for the same key.
# Ticket: #2002
# Bug still in: 4.11-103-gc8d51b4
# Bug introduced with commit bf3cd41b5ddf1e757515ab5fbf811be56e5f69cc
use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym Print nop Print
bindsym --release Control+Print nop Control+Print

# see issue #2442
bindsym Mod1+b nop Mod1+b
bindsym --release Mod1+Shift+b nop Mod1+Shift+b release
EOT
use i3test::XTEST;
use ExtUtils::PkgConfig;

SKIP: {
    skip "libxcb-xkb too old (need >= 1.11)", 1 unless
        ExtUtils::PkgConfig->atleast_version('xcb-xkb', '1.11');

is(listen_for_binding(
    sub {
        xtest_key_press(107); # Print
        xtest_key_release(107); # Print
        xtest_sync_with_i3;
    },
    ),
    'Print',
    'triggered the "Print" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(37); # Control_L
        xtest_key_press(107); # Print
        xtest_key_release(107); # Print
        xtest_key_release(37); # Control_L
        xtest_sync_with_i3;
    },
    ),
    'Control+Print',
    'triggered the "Control+Print" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(64); # Alt_L
        xtest_key_press(56); # b
        xtest_key_release(56); # b
        xtest_key_release(64); # Alt_L
        xtest_sync_with_i3;
    },
    ),
    'Mod1+b',
    'triggered the "Mod1+b" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(64); # Alt_L
        xtest_key_press(50); # Shift_L
        xtest_key_press(56); # b
        xtest_key_release(56); # b
        xtest_key_release(50); # Shift_L
        xtest_key_release(64); # Alt_L
        xtest_sync_with_i3;
    },
    ),
    'Mod1+Shift+b release',
    'triggered the "Mod1+Shift+b" release keybinding');

}

done_testing;
