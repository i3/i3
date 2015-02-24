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
# Tests the standalone parser binary to see if it calls the right code when
# confronted with various commands, if it prints proper error messages for
# wrong commands and if it terminates in every case.
#
use i3test i3_autostart => 0;
use IPC::Run qw(run);

sub parser_calls {
    my ($command) = @_;

    my $stdout;
    run [ '../test.config_parser', $command ],
        '>/dev/null',
        '2>', \$stdout;
    # TODO: use a timeout, so that we can error out if it doesn’t terminate

    # Filter out all debugging output.
    my @lines = split("\n", $stdout);
    @lines = grep { not /^# / } @lines;

    # The criteria management calls are irrelevant and not what we want to test
    # in the first place.
    @lines = grep { !(/cfg_criteria_init/ || /cfg_criteria_pop_state/) } @lines;
    return join("\n", @lines) . "\n";
}

my $config = <<'EOT';
mode "meh" {
    bindsym Mod1 + Shift +   x resize grow
    bindcode Mod1+44 resize shrink
    bindsym --release Mod1+x exec foo
}
EOT

my $expected = <<'EOT';
cfg_enter_mode(meh)
cfg_mode_binding(bindsym, Mod1,Shift, x, (null), resize grow)
cfg_mode_binding(bindcode, Mod1, 44, (null), resize shrink)
cfg_mode_binding(bindsym, Mod1, x, --release, exec foo)
EOT

is(parser_calls($config),
   $expected,
   'single number (move workspace 3) ok');

################################################################################
# exec and exec_always
################################################################################

$config = <<'EOT';
exec geeqie
exec --no-startup-id /tmp/foo.sh
exec_always firefox
exec_always --no-startup-id /tmp/bar.sh
EOT

$expected = <<'EOT';
cfg_exec(exec, (null), geeqie)
cfg_exec(exec, --no-startup-id, /tmp/foo.sh)
cfg_exec(exec_always, (null), firefox)
cfg_exec(exec_always, --no-startup-id, /tmp/bar.sh)
EOT

is(parser_calls($config),
   $expected,
   'exec okay');

################################################################################
# for_window
################################################################################

$config = <<'EOT';
for_window [class="^Chrome"] floating enable
EOT

$expected = <<'EOT';
cfg_criteria_add(class, ^Chrome)
cfg_for_window(floating enable)
EOT

is(parser_calls($config),
   $expected,
   'for_window okay');

################################################################################
# assign
################################################################################

$config = <<'EOT';
assign [class="^Chrome"] 4
assign [class="^Chrome"] named workspace
assign [class="^Chrome"] "quoted named workspace"
assign [class="^Chrome"] → "quoted named workspace"
EOT

$expected = <<'EOT';
cfg_criteria_add(class, ^Chrome)
cfg_assign(4)
cfg_criteria_add(class, ^Chrome)
cfg_assign(named workspace)
cfg_criteria_add(class, ^Chrome)
cfg_assign(quoted named workspace)
cfg_criteria_add(class, ^Chrome)
cfg_assign(quoted named workspace)
EOT

is(parser_calls($config),
   $expected,
   'for_window okay');

################################################################################
# floating_minimum_size / floating_maximum_size
################################################################################

$config = <<'EOT';
floating_minimum_size 80x55
floating_minimum_size 80    x  55  
floating_maximum_size 73 x 10
EOT

$expected = <<'EOT';
cfg_floating_minimum_size(80, 55)
cfg_floating_minimum_size(80, 55)
cfg_floating_maximum_size(73, 10)
EOT

is(parser_calls($config),
   $expected,
   'floating_minimum_size ok');

################################################################################
# popup_during_fullscreen
################################################################################

$config = <<'EOT';
popup_during_fullscreen ignore
popup_during_fullscreen leave_fullscreen
popup_during_fullscreen SMArt
EOT

$expected = <<'EOT';
cfg_popup_during_fullscreen(ignore)
cfg_popup_during_fullscreen(leave_fullscreen)
cfg_popup_during_fullscreen(smart)
EOT

is(parser_calls($config),
   $expected,
   'popup_during_fullscreen ok');


################################################################################
# floating_modifier
################################################################################

$config = <<'EOT';
floating_modifier Mod1
floating_modifier mOd1
EOT

$expected = <<'EOT';
cfg_floating_modifier(Mod1)
cfg_floating_modifier(Mod1)
EOT

is(parser_calls($config),
   $expected,
   'floating_modifier ok');

################################################################################
# default_orientation
################################################################################

$config = <<'EOT';
default_orientation horizontal
default_orientation vertical
default_orientation auto
EOT

$expected = <<'EOT';
cfg_default_orientation(horizontal)
cfg_default_orientation(vertical)
cfg_default_orientation(auto)
EOT

is(parser_calls($config),
   $expected,
   'default_orientation ok');

################################################################################
# workspace_layout
################################################################################

$config = <<'EOT';
workspace_layout default
workspace_layout stacked
workspace_layout stacking
workspace_layout tabbed
EOT

$expected = <<'EOT';
cfg_workspace_layout(default)
cfg_workspace_layout(stacked)
cfg_workspace_layout(stacking)
cfg_workspace_layout(tabbed)
EOT

is(parser_calls($config),
   $expected,
   'workspace_layout ok');

################################################################################
# workspace assignments, with trailing whitespace (ticket #921)
################################################################################

$config = <<'EOT';
workspace "3" output DP-1 
workspace "3" output     	VGA-1	
EOT

$expected = <<'EOT';
cfg_workspace(3, DP-1)
cfg_workspace(3, VGA-1)
EOT

is(parser_calls($config),
   $expected,
   'workspace assignment ok');

################################################################################
# new_window
################################################################################

$config = <<'EOT';
new_window 1pixel
new_window normal
new_window none
new_float 1pixel
new_float normal
new_float none
EOT

$expected = <<'EOT';
cfg_new_window(new_window, 1pixel, -1)
cfg_new_window(new_window, normal, 2)
cfg_new_window(new_window, none, -1)
cfg_new_window(new_float, 1pixel, -1)
cfg_new_window(new_float, normal, 2)
cfg_new_window(new_float, none, -1)
EOT

is(parser_calls($config),
   $expected,
   'new_window ok');

################################################################################
# hide_edge_borders
################################################################################

$config = <<'EOT';
hide_edge_borders none
hide_edge_borders vertical
hide_edge_borders horizontal
hide_edge_borders both
EOT

$expected = <<'EOT';
cfg_hide_edge_borders(none)
cfg_hide_edge_borders(vertical)
cfg_hide_edge_borders(horizontal)
cfg_hide_edge_borders(both)
EOT

is(parser_calls($config),
   $expected,
   'hide_edge_borders ok');

################################################################################
# focus_follows_mouse
################################################################################

$config = <<'EOT';
focus_follows_mouse yes
focus_follows_mouse no
EOT

$expected = <<'EOT';
cfg_focus_follows_mouse(yes)
cfg_focus_follows_mouse(no)
EOT

is(parser_calls($config),
   $expected,
   'focus_follows_mouse ok');

################################################################################
# mouse_warping
################################################################################

$config = <<'EOT';
mouse_warping output
mouse_warping none
EOT

$expected = <<'EOT';
cfg_mouse_warping(output)
cfg_mouse_warping(none)
EOT

is(parser_calls($config),
   $expected,
   'mouse_warping ok');

################################################################################
# force_display_urgency_hint
################################################################################

is(parser_calls('force_display_urgency_hint 300'),
   "cfg_force_display_urgency_hint(300)\n",
   'force_display_urgency_hint ok');

is(parser_calls('force_display_urgency_hint 500 ms'),
   "cfg_force_display_urgency_hint(500)\n",
   'force_display_urgency_hint ok');

is(parser_calls('force_display_urgency_hint 700ms'),
   "cfg_force_display_urgency_hint(700)\n",
   'force_display_urgency_hint ok');

$config = <<'EOT';
force_display_urgency_hint 300
force_display_urgency_hint 500 ms
force_display_urgency_hint 700ms
force_display_urgency_hint 700
EOT

$expected = <<'EOT';
cfg_force_display_urgency_hint(300)
cfg_force_display_urgency_hint(500)
cfg_force_display_urgency_hint(700)
cfg_force_display_urgency_hint(700)
EOT

is(parser_calls($config),
   $expected,
   'force_display_urgency_hint ok');

################################################################################
# workspace
################################################################################

$config = <<'EOT';
workspace 3 output VGA-1
workspace "4: output" output VGA-2
workspace bleh output LVDS1/I_1
EOT

$expected = <<'EOT';
cfg_workspace(3, VGA-1)
cfg_workspace(4: output, VGA-2)
cfg_workspace(bleh, LVDS1/I_1)
EOT

is(parser_calls($config),
   $expected,
   'workspace ok');

################################################################################
# ipc-socket
################################################################################

$config = <<'EOT';
ipc-socket /tmp/i3.sock
ipc_socket ~/.i3/i3.sock
EOT

$expected = <<'EOT';
cfg_ipc_socket(/tmp/i3.sock)
cfg_ipc_socket(~/.i3/i3.sock)
EOT

is(parser_calls($config),
   $expected,
   'ipc-socket ok');

################################################################################
# colors
################################################################################

$config = <<'EOT';
client.focused          #4c7899 #285577 #ffffff #2e9ef4
client.focused_inactive #333333 #5f676a #ffffff #484e50
client.unfocused        #333333 #222222 #888888 #292d2e
client.urgent           #2f343a #900000 #ffffff #900000
EOT

$expected = <<'EOT';
cfg_color(client.focused, #4c7899, #285577, #ffffff, #2e9ef4)
cfg_color(client.focused_inactive, #333333, #5f676a, #ffffff, #484e50)
cfg_color(client.unfocused, #333333, #222222, #888888, #292d2e)
cfg_color(client.urgent, #2f343a, #900000, #ffffff, #900000)
EOT

is(parser_calls($config),
   $expected,
   'colors ok');

################################################################################
# Verify that errors don’t harm subsequent valid statements
################################################################################

$config = <<'EOT';
hide_edge_border both
client.focused          #4c7899 #285577 #ffffff #2e9ef4
EOT

my $expected_all_tokens = <<'EOT';
ERROR: CONFIG: Expected one of these tokens: <end>, '#', 'set', 'bindsym', 'bindcode', 'bind', 'bar', 'font', 'mode', 'floating_minimum_size', 'floating_maximum_size', 'floating_modifier', 'default_orientation', 'workspace_layout', 'new_window', 'new_float', 'hide_edge_borders', 'for_window', 'assign', 'focus_follows_mouse', 'mouse_warping', 'force_focus_wrapping', 'force_xinerama', 'force-xinerama', 'workspace_auto_back_and_forth', 'fake_outputs', 'fake-outputs', 'force_display_urgency_hint', 'workspace', 'ipc_socket', 'ipc-socket', 'restart_state', 'popup_during_fullscreen', 'exec_always', 'exec', 'client.background', 'client.focused_inactive', 'client.focused', 'client.unfocused', 'client.urgent'
EOT

my $expected_end = <<'EOT';
ERROR: CONFIG: (in file <stdin>)
ERROR: CONFIG: Line   1: hide_edge_border both
ERROR: CONFIG:           ^^^^^^^^^^^^^^^^^^^^^
ERROR: CONFIG: Line   2: client.focused          #4c7899 #285577 #ffffff #2e9ef4
cfg_color(client.focused, #4c7899, #285577, #ffffff, #2e9ef4)
EOT

$expected = $expected_all_tokens . $expected_end;

is(parser_calls($config),
   $expected,
   'errors dont harm subsequent statements');

$config = <<'EOT';
hide_edge_borders FOOBAR
client.focused          #4c7899 #285577 #ffffff #2e9ef4
EOT

$expected = <<'EOT';
ERROR: CONFIG: Expected one of these tokens: 'none', 'vertical', 'horizontal', 'both', '1', 'yes', 'true', 'on', 'enable', 'active'
ERROR: CONFIG: (in file <stdin>)
ERROR: CONFIG: Line   1: hide_edge_borders FOOBAR
ERROR: CONFIG:                             ^^^^^^
ERROR: CONFIG: Line   2: client.focused          #4c7899 #285577 #ffffff #2e9ef4
cfg_color(client.focused, #4c7899, #285577, #ffffff, #2e9ef4)
EOT

is(parser_calls($config),
   $expected,
   'errors dont harm subsequent statements');

################################################################################
# Regression: semicolons end comments, but shouldn’t
################################################################################

$config = <<'EOT';
# "foo" client.focused          #4c7899 #285577 #ffffff #2e9ef4
EOT

$expected = <<'EOT';

EOT

is(parser_calls($config),
   $expected,
   'semicolon does not end a comment line');

################################################################################
# Error message with 2+2 lines of context
################################################################################

$config = <<'EOT';
# i3 config file (v4)

font foobar

unknown qux

# yay
# this should not show up
EOT

my $expected_head = <<'EOT';
cfg_font(foobar)
EOT

my $expected_tail = <<'EOT';
ERROR: CONFIG: (in file <stdin>)
ERROR: CONFIG: Line   3: font foobar
ERROR: CONFIG: Line   4: 
ERROR: CONFIG: Line   5: unknown qux
ERROR: CONFIG:           ^^^^^^^^^^^
ERROR: CONFIG: Line   6: 
ERROR: CONFIG: Line   7: # yay
EOT

$expected = $expected_head . $expected_all_tokens . $expected_tail;

is(parser_calls($config),
   $expected,
   'error message (2+2 context) ok');

################################################################################
# Error message with 0+0 lines of context
################################################################################

$config = <<'EOT';
unknown qux
EOT

$expected_tail = <<'EOT';
ERROR: CONFIG: (in file <stdin>)
ERROR: CONFIG: Line   1: unknown qux
ERROR: CONFIG:           ^^^^^^^^^^^
EOT

$expected = $expected_all_tokens . $expected_tail;

is(parser_calls($config),
   $expected,
   'error message (0+0 context) ok');

################################################################################
# Error message with 1+0 lines of context
################################################################################

$config = <<'EOT';
# context before
unknown qux
EOT

$expected_tail = <<'EOT';
ERROR: CONFIG: (in file <stdin>)
ERROR: CONFIG: Line   1: # context before
ERROR: CONFIG: Line   2: unknown qux
ERROR: CONFIG:           ^^^^^^^^^^^
EOT

$expected = $expected_all_tokens . $expected_tail;

is(parser_calls($config),
   $expected,
   'error message (1+0 context) ok');

################################################################################
# Error message with 0+1 lines of context
################################################################################

$config = <<'EOT';
unknown qux
# context after
EOT

$expected_tail = <<'EOT';
ERROR: CONFIG: (in file <stdin>)
ERROR: CONFIG: Line   1: unknown qux
ERROR: CONFIG:           ^^^^^^^^^^^
ERROR: CONFIG: Line   2: # context after
EOT

$expected = $expected_all_tokens . $expected_tail;

is(parser_calls($config),
   $expected,
   'error message (0+1 context) ok');

################################################################################
# Error message with 0+2 lines of context
################################################################################

$config = <<'EOT';
unknown qux
# context after
# context 2 after
EOT

$expected_tail = <<'EOT';
ERROR: CONFIG: (in file <stdin>)
ERROR: CONFIG: Line   1: unknown qux
ERROR: CONFIG:           ^^^^^^^^^^^
ERROR: CONFIG: Line   2: # context after
ERROR: CONFIG: Line   3: # context 2 after
EOT

$expected = $expected_all_tokens . $expected_tail;

is(parser_calls($config),
   $expected,
   'error message (0+2 context) ok');

################################################################################
# Error message within mode blocks
################################################################################

$config = <<'EOT';
mode "yo" {
    bindsym x resize shrink left
    unknown qux
}
EOT

$expected = <<'EOT';
cfg_enter_mode(yo)
cfg_mode_binding(bindsym, (null), x, (null), resize shrink left)
ERROR: CONFIG: Expected one of these tokens: <end>, '#', 'set', 'bindsym', 'bindcode', 'bind', '}'
ERROR: CONFIG: (in file <stdin>)
ERROR: CONFIG: Line   1: mode "yo" {
ERROR: CONFIG: Line   2:     bindsym x resize shrink left
ERROR: CONFIG: Line   3:     unknown qux
ERROR: CONFIG:               ^^^^^^^^^^^
ERROR: CONFIG: Line   4: }
EOT

is(parser_calls($config),
   $expected,
   'error message (mode block) ok');

################################################################################
# Error message within bar blocks
################################################################################

$config = <<'EOT';
bar {
    output LVDS-1
    unknown qux
}
EOT

$expected = <<'EOT';
cfg_bar_output(LVDS-1)
ERROR: CONFIG: Expected one of these tokens: <end>, '#', 'set', 'i3bar_command', 'status_command', 'socket_path', 'mode', 'hidden_state', 'id', 'modifier', 'position', 'output', 'tray_output', 'font', 'binding_mode_indicator', 'workspace_buttons', 'strip_workspace_numbers', 'verbose', 'colors', '}'
ERROR: CONFIG: (in file <stdin>)
ERROR: CONFIG: Line   1: bar {
ERROR: CONFIG: Line   2:     output LVDS-1
ERROR: CONFIG: Line   3:     unknown qux
ERROR: CONFIG:               ^^^^^^^^^^^
ERROR: CONFIG: Line   4: }
cfg_bar_finish()
EOT

is(parser_calls($config),
   $expected,
   'error message (bar block) ok');

done_testing;
