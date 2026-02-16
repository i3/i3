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
# Verify that i3 does not crash when various commands are run with a for_window
# rule existing, even in an empty workspace.
# Ticket: #6561
# Bug still in: 4.25-6-g0e2e8290
use i3test i3_config => <<EOT;
for_window [class=xxx] nop
EOT

# Table of commands and their expected success status (1 = success, 0 = failure)
# Commands are tested in an empty workspace to verify no crashes occur.
my %commands = (
    # nop command - always succeeds
    'nop' => 1,
    'nop test comment' => 1,

    # floating commands
    'floating toggle' => 1,
    'floating enable' => 1,
    'floating disable' => 1,

    # split commands
    'split h' => 1,
    'split v' => 1,
    'split t' => 1,
    'split horizontal' => 1,
    'split vertical' => 1,
    'split toggle' => 1,

    # layout commands
    'layout default' => 1,
    'layout tabbed' => 1,
    'layout stacking' => 1,
    'layout stacked' => 1,
    'layout splitv' => 1,
    'layout splith' => 1,
    'layout toggle' => 1,
    'layout toggle split' => 1,
    'layout toggle all' => 1,

    # fullscreen commands
    'fullscreen toggle' => 1,
    'fullscreen enable' => 1,
    'fullscreen disable' => 1,
    'fullscreen toggle global' => 1,
    'fullscreen enable global' => 1,

    # focus direction commands
    'focus left' => 1,
    'focus right' => 1,
    'focus up' => 1,
    'focus down' => 1,
    'focus prev' => 1,
    'focus next' => 1,
    'focus prev sibling' => 1,
    'focus next sibling' => 1,

    # focus level commands - fail in empty workspace (no parent/child to focus)
    'focus parent' => 0,
    'focus child' => 0,

    # focus mode commands - fail because no matching container type exists
    'focus tiling' => 0,
    'focus floating' => 0,
    'focus mode_toggle' => 0,

    # kill commands - succeed even with nothing to kill
    'kill' => 1,
    'kill window' => 1,
    'kill client' => 1,
    '[all] kill' => 1,

    # border commands
    'border normal' => 1,
    'border pixel' => 1,
    'border pixel 2' => 1,
    'border none' => 1,
    'border toggle' => 1,
    'border 1pixel' => 1,

    # sticky commands - succeed but skip containers without windows
    'sticky enable' => 1,
    'sticky disable' => 1,
    'sticky toggle' => 1,

    # mark/unmark commands
    'mark testmark' => 1,
    'mark --add testmark2' => 1,
    'mark --replace testmark3' => 1,
    'mark --toggle testmark4' => 1,
    'unmark' => 1,
    'unmark testmark' => 1,

    'rename workspace to xxx' => 1,
    'rename workspace abc to yyy' => 0,

    # resize commands - fail in empty workspace (no container to resize)
    'resize grow width 10 px' => 0,
    'resize shrink width 10 px' => 0,
    'resize grow height 10 px' => 0,
    'resize shrink height 10 px' => 0,
    'resize grow width 10 px or 5 ppt' => 0,
    'resize set 50 ppt 50 ppt' => 0,

    # move direction commands
    'move left' => 1,
    'move right' => 1,
    'move up' => 1,
    'move down' => 1,
    'move left 10 px' => 1,
    'move right 10 ppt' => 1,

    # move to scratchpad
    'move scratchpad' => 1,

    # scratchpad show - fails when no scratchpad window exists
    'scratchpad show' => 0,

    # title format
    'title_format "%title"' => 1,
    'title_format "test: %title"' => 1,

    # title window icon
    'title_window_icon on' => 1,
    'title_window_icon off' => 1,
    'title_window_icon toggle' => 1,
    'title_window_icon padding 3' => 1,

    # mode command
    'mode "default"' => 1,

    # gaps commands
    'gaps inner current set 0' => 1,
    'gaps outer current set 0' => 1,
    'gaps inner all set 0' => 1,
    'gaps inner current plus 5' => 1,
    'gaps inner current minus 5' => 1,
    'gaps inner current toggle 10' => 1,
    'gaps top current set 0' => 1,
    'gaps right current set 0' => 1,
    'gaps bottom current set 0' => 1,
    'gaps left current set 0' => 1,
    'gaps horizontal current set 0' => 1,
    'gaps vertical current set 0' => 1,

    'workspace xxx' => 1,
    'workspace back_and_forth' => 1,
    'workspace next' => 1,
    'workspace prev' => 1,
    'workspace next_on_output' => 1,
    'workspace prev_on_output' => 1,

    'swap container with id 123' => 0,
    'swap container with con_id 123' => 0,
    'swap container with mark swap_mark' => 0,
);

for my $command (sort keys %commands) {
    my $expected_success = $commands{$command};

    subtest "command: $command" => sub {
        fresh_workspace;

        my $result = cmd $command;
        does_i3_live;

        # Check command success/failure matches expectation
        my $actual_success = $result->[0]->{success} ? 1 : 0;
        is($actual_success, $expected_success,
           $expected_success ? 'command succeeded as expected' : 'command failed as expected');
    };
}

done_testing;
