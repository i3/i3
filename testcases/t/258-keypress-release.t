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
# Verifies that --release key bindings are not shadowed by non-release key
# bindings for the same key.
# Ticket: #2002
# Bug still in: 4.11-103-gc8d51b4
# Bug introduced with commit bf3cd41b5ddf1e757515ab5fbf811be56e5f69cc
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym Print nop Print
bindsym --release Control+Print nop Control+Print

# see issue #2442
bindsym Mod1+b nop Mod1+b
bindsym --release Mod1+Shift+b nop Mod1+Shift+b release

bindsym --release Shift+x nop Shift+x

# see issue #2733
# 133 == Mod4
bindcode 133 nop 133
bindcode --release 133 nop 133 release

mode "a_mode" {
    # 27 == r
    bindcode 27 --release mode "default"
}
bindsym Mod1+r mode "a_mode"
bindcode 27 nop do not receive
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

is(listen_for_binding(
        sub {
            xtest_key_press(50); # Shift
            xtest_key_press(53); # x
            xtest_key_release(53); # x
            xtest_key_release(50); # Shift
            xtest_sync_with_i3;
        },
        ),
       'Shift+x',
       'triggered the "Shift+x" keybinding by releasing x first');

is(listen_for_binding(
        sub {
            xtest_key_press(50); # Shift
            xtest_key_press(53); # x
            xtest_key_release(50); # Shift
            xtest_key_release(53);  # x
            xtest_sync_with_i3;
        },
        ),
       'Shift+x',
       'triggered the "Shift+x" keybinding by releasing Shift first');

is(listen_for_binding(
    sub {
        xtest_key_press(133);
        xtest_sync_with_i3;
    },
    ),
    '133',
    'triggered the 133 keycode press binding');

is(listen_for_binding(
    sub {
        xtest_key_release(133);
        xtest_sync_with_i3;
    },
    ),
    '133 release',
    'triggered the 133 keycode release binding');

for my $i (1 .. 2) {
    is(listen_for_binding(
        sub {
            xtest_key_press(64); # Alt_l
            xtest_key_press(27); # r
            xtest_key_release(27); # r
            xtest_key_release(64); # Alt_l
            xtest_sync_with_i3;
        },
        ),
        'mode "a_mode"',
        "switched to mode \"a_mode\" $i/2");

    is(listen_for_binding(
        sub {
            xtest_key_press(27); # r
            xtest_key_release(27); # r
            xtest_sync_with_i3;
        },
        ),
        'mode "default"',
        "switched back to default $i/2");
}

}

done_testing;
