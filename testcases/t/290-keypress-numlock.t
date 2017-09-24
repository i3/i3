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
# Verifies that one can bind on numpad keys in different numlock states.
# Ticket: #2346
# Bug still in: 4.12-78-g85bb324
use i3test i3_autostart => 0;
use i3test::XTEST;
use ExtUtils::PkgConfig;

SKIP: {
    skip "libxcb-xkb too old (need >= 1.11)", 1 unless
        ExtUtils::PkgConfig->atleast_version('xcb-xkb', '1.11');

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

# Same key, different numlock states.
bindsym Mod2+KP_1 nop KP_1
bindsym KP_End nop KP_End

# Binding which should work with numlock and without.
bindsym Mod4+a nop a

# Binding which should work with numlock and without, see issue #2418.
bindsym Escape nop Escape

# Binding which should work with numlock and without, see issue #2418.
bindsym Shift+Escape nop Shift+Escape

# Binding which should work with numlock and without, see issue #2418.
bindsym Mod1+Shift+q nop Mod1+Shift+q

# Binding which should work with numlock and without, see issue #2559.
bindcode 39 nop s
EOT

my $pid = launch_with_config($config);

is(listen_for_binding(
    sub {
        xtest_key_press(87); # KP_End
        xtest_key_release(87); # KP_End
        xtest_sync_with_i3;
    },
    ),
   'KP_End',
   'triggered the "KP_End" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(77); # enable Num_Lock
        xtest_key_release(77); # enable Num_Lock
        xtest_key_press(87); # KP_1
        xtest_key_release(87); # KP_1
        xtest_key_press(77); # disable Num_Lock
        xtest_key_release(77); # disable Num_Lock
        xtest_sync_with_i3;
    },
    ),
   'KP_1',
   'triggered the "KP_1" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(133); # Super_L
        xtest_key_press(38); # a
        xtest_key_release(38); # a
        xtest_key_release(133); # Super_L
        xtest_sync_with_i3;
    },
    ),
   'a',
   'triggered the "a" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(77); # enable Num_Lock
        xtest_key_release(77); # enable Num_Lock
        xtest_key_press(133); # Super_L
        xtest_key_press(38); # a
        xtest_key_release(38); # a
        xtest_key_release(133); # Super_L
        xtest_key_press(77); # disable Num_Lock
        xtest_key_release(77); # disable Num_Lock
        xtest_sync_with_i3;
    },
    ),
   'a',
   'triggered the "a" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(9); # Escape
        xtest_key_release(9); # Escape
        xtest_sync_with_i3;
    },
    ),
   'Escape',
   'triggered the "Escape" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(77); # enable Num_Lock
        xtest_key_release(77); # enable Num_Lock
        xtest_key_press(9); # Escape
        xtest_key_release(9); # Escape
        xtest_key_press(77); # disable Num_Lock
        xtest_key_release(77); # disable Num_Lock
        xtest_sync_with_i3;
    },
    ),
   'Escape',
   'triggered the "Escape" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(50); # Shift_L
        xtest_key_press(9); # Escape
        xtest_key_release(9); # Escape
        xtest_key_release(50); # Shift_L
        xtest_sync_with_i3;
    },
    ),
   'Shift+Escape',
   'triggered the "Escape" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(77); # enable Num_Lock
        xtest_key_release(77); # enable Num_Lock
        xtest_key_press(50); # Shift_L
        xtest_key_press(9); # Escape
        xtest_key_release(9); # Escape
        xtest_key_release(50); # Shift_L
        xtest_key_press(77); # disable Num_Lock
        xtest_key_release(77); # disable Num_Lock
        xtest_sync_with_i3;
    },
    ),
   'Shift+Escape',
   'triggered the "Escape" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(50); # Shift_L
        xtest_key_press(64); # Alt_L
        xtest_key_press(24); # q
        xtest_key_release(24); # q
        xtest_key_release(64); # Alt_L
        xtest_key_release(50); # Shift_L
        xtest_sync_with_i3;
    },
    ),
   'Mod1+Shift+q',
   'triggered the "Mod1+Shift+q" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(77); # enable Num_Lock
        xtest_key_release(77); # enable Num_Lock
        xtest_key_press(50); # Shift_L
        xtest_key_press(64); # Alt_L
        xtest_key_press(24); # q
        xtest_key_release(24); # q
        xtest_key_release(64); # Alt_L
        xtest_key_release(50); # Shift_L
        xtest_key_press(77); # disable Num_Lock
        xtest_key_release(77); # disable Num_Lock
        xtest_sync_with_i3;
    },
    ),
   'Mod1+Shift+q',
   'triggered the "Mod1+Shift+q" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(39); # s
        xtest_key_release(39); # s
        xtest_sync_with_i3;
    },
    ),
   's',
   'triggered the "s" keybinding without Num_Lock');

is(listen_for_binding(
    sub {
        xtest_key_press(77); # enable Num_Lock
        xtest_key_release(77); # enable Num_Lock
        xtest_key_press(39); # s
        xtest_key_release(39); # s
        xtest_key_press(77); # disable Num_Lock
        xtest_key_release(77); # disable Num_Lock
        xtest_sync_with_i3;
    },
    ),
   's',
   'triggered the "s" keybinding with Num_Lock');

exit_gracefully($pid);

################################################################################
# Verify bindings for modifiers work
################################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym Mod4+Return nop Return

# Binding which should work with numlock and without, see issue #2559.
bindcode --release 133 nop Super_L
EOT

$pid = launch_with_config($config);

is(listen_for_binding(
    sub {
        xtest_key_press(133); # Super_L
        xtest_key_release(133); # Super_L
        xtest_sync_with_i3;
    },
    ),
   'Super_L',
   'triggered the "Super_L" keybinding without Num_Lock');

is(listen_for_binding(
    sub {
        xtest_key_press(77); # enable Num_Lock
        xtest_key_release(77); # enable Num_Lock
        xtest_key_press(133); # Super_L
        xtest_key_release(133); # Super_L
        xtest_key_press(77); # disable Num_Lock
        xtest_key_release(77); # disable Num_Lock
        xtest_sync_with_i3;
    },
    ),
   'Super_L',
   'triggered the "Super_L" keybinding with Num_Lock');

is(listen_for_binding(
    sub {
        xtest_key_press(133); # Super_L
        xtest_key_press(36); # Return
        xtest_key_release(36); # Return
        xtest_key_release(133); # Super_L
        xtest_sync_with_i3;
    },
    ),
   'Return',
   'triggered the "Return" keybinding without Num_Lock');

is(listen_for_binding(
    sub {
        xtest_key_press(77); # enable Num_Lock
        xtest_key_release(77); # enable Num_Lock
        xtest_key_press(133); # Super_L
        xtest_key_press(36); # Return
        xtest_key_release(36); # Return
        xtest_key_release(133); # Super_L
        xtest_key_press(77); # disable Num_Lock
        xtest_key_release(77); # disable Num_Lock
        xtest_sync_with_i3;
    },
    ),
   'Return',
   'triggered the "Return" keybinding with Num_Lock');

exit_gracefully($pid);

################################################################################
# Verify the binding is only triggered for KP_End, not KP_1
################################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym KP_End nop KP_End
bindcode 88 nop KP_Down
EOT

$pid = launch_with_config($config);

is(listen_for_binding(
    sub {
        xtest_key_press(87); # KP_End
        xtest_key_release(87); # KP_End
        xtest_sync_with_i3;
    },
    ),
   'KP_End',
   'triggered the "KP_End" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(88); # KP_Down
        xtest_key_release(88); # KP_Down
        xtest_sync_with_i3;
    },
    ),
   'KP_Down',
   'triggered the "KP_Down" keybinding');

my @unexpected = events_for(
    sub {
        xtest_key_press(77); # enable Num_Lock
        xtest_key_release(77); # enable Num_Lock
        xtest_key_press(87); # KP_1
        xtest_key_release(87); # KP_1
        xtest_key_press(77); # disable Num_Lock
        xtest_key_release(77); # disable Num_Lock
        xtest_sync_with_i3;
    },
    'binding');
is(scalar @unexpected, 0, 'Did not trigger the KP_End keybinding with KP_1');

my @unexpected2 = events_for(
    sub {
        xtest_key_press(77); # enable Num_Lock
        xtest_key_release(77); # enable Num_Lock
        xtest_key_press(88); # KP_2
        xtest_key_release(88); # KP_2
        xtest_key_press(77); # disable Num_Lock
        xtest_key_release(77); # disable Num_Lock
        xtest_sync_with_i3;
    },
    'binding');

is(scalar @unexpected2, 0, 'Did not trigger the KP_Down keybinding with KP_2');

# TODO: This test does not verify that i3 does _NOT_ grab keycode 87 with Mod2.

exit_gracefully($pid);

################################################################################
# Verify mouse bindings are unaffected by NumLock
################################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym --whole-window button4 nop button4
EOT

$pid = launch_with_config($config);

my $win = open_window;

is(listen_for_binding(
    sub {
        xtest_key_press(77); # enable Num_Lock
        xtest_key_release(77); # enable Num_Lock
        xtest_button_press(4, 50, 50);
        xtest_button_release(4, 50, 50);
        xtest_key_press(77); # disable Num_Lock
        xtest_key_release(77); # disable Num_Lock
        xtest_sync_with_i3;
    },
    ),
   'button4',
   'triggered the button4 keybinding with NumLock');

is(listen_for_binding(
    sub {
        xtest_button_press(4, 50, 50);
        xtest_button_release(4, 50, 50);
        xtest_sync_with_i3;
    },
    ),
   'button4',
   'triggered the button4 keybinding without NumLock');

exit_gracefully($pid);

}

done_testing;
