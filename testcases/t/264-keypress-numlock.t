#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
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
EOT

my $pid = launch_with_config($config);

start_binding_capture;

is(listen_for_binding(
    sub {
        xtest_key_press(87); # KP_End
        xtest_key_release(87); # KP_End
    },
    ),
   'KP_End',
   'triggered the "KP_End" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(77); # Num_Lock
        xtest_key_press(87); # KP_1
        xtest_key_release(87); # KP_1
        xtest_key_release(77); # Num_Lock
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
    },
    ),
   'a',
   'triggered the "a" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(77); # Num_Lock
        xtest_key_press(133); # Super_L
        xtest_key_press(38); # a
        xtest_key_release(38); # a
        xtest_key_release(133); # Super_L
        xtest_key_release(77); # Num_Lock
    },
    ),
   'a',
   'triggered the "a" keybinding');


sync_with_i3;
is(scalar @i3test::XTEST::binding_events, 4, 'Received exactly 4 binding events');

exit_gracefully($pid);

################################################################################
# Verify the binding is only triggered for KP_End, not KP_1
################################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bindsym KP_End nop KP_End
EOT

$pid = launch_with_config($config);

start_binding_capture;

is(listen_for_binding(
    sub {
        xtest_key_press(87); # KP_End
        xtest_key_release(87); # KP_End
    },
    ),
   'KP_End',
   'triggered the "KP_End" keybinding');

is(listen_for_binding(
    sub {
        xtest_key_press(77); # Num_Lock
        xtest_key_press(87); # KP_1
        xtest_key_release(87); # KP_1
        xtest_key_release(77); # Num_Lock
    },
    ),
   'timeout',
   'Did not trigger the KP_End keybinding with KP_1');

# TODO: This test does not verify that i3 does _NOT_ grab keycode 87 with Mod2.

sync_with_i3;
is(scalar @i3test::XTEST::binding_events, 5, 'Received exactly 5 binding events');

exit_gracefully($pid);

}

done_testing;
