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
# Checks that the bar config is parsed correctly.
#

use i3test i3_autostart => 0;

#####################################################################
# test a config without any bars
#####################################################################

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $pid = launch_with_config($config);

my $i3 = i3(get_socket_path(0));
my $bars = $i3->get_bar_config()->recv;
is(@$bars, 0, 'no bars configured');

exit_gracefully($pid);

#####################################################################
# now provide a simple bar configuration
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bar {
    # Start a default instance of i3bar which provides workspace buttons.
    # Additionally, i3status will provide a statusline.
    status_command i3status --foo
}
EOT

$pid = launch_with_config($config);

$i3 = i3(get_socket_path(0));
$bars = $i3->get_bar_config()->recv;
is(@$bars, 1, 'one bar configured');

my $bar_id = shift @$bars;

my $bar_config = $i3->get_bar_config($bar_id)->recv;
is($bar_config->{status_command}, 'i3status --foo', 'status_command correct');
ok(!$bar_config->{verbose}, 'verbose off by default');
ok($bar_config->{workspace_buttons}, 'workspace buttons enabled per default');
is($bar_config->{workspace_min_width}, 0, 'workspace_min_width ok');
ok($bar_config->{binding_mode_indicator}, 'mode indicator enabled per default');
is($bar_config->{mode}, 'dock', 'dock mode by default');
is($bar_config->{position}, 'bottom', 'position bottom by default');
is($bar_config->{tray_padding}, 2, 'tray_padding ok');

#####################################################################
# ensure that reloading cleans up the old bar configs
#####################################################################

cmd 'reload';
$bars = $i3->get_bar_config()->recv;
is(@$bars, 1, 'still one bar configured');

exit_gracefully($pid);

#####################################################################
# validate a more complex configuration
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bar {
    # Start a default instance of i3bar which does not provide
    # workspace buttons.
    # Additionally, i3status will provide a statusline.
    status_command i3status --bar

    output HDMI1
    output HDMI2

    tray_output LVDS1
    tray_output HDMI2
    tray_padding 0
    position top
    mode dock
    font Terminus
    workspace_buttons no
    workspace_min_width 30
    binding_mode_indicator no
    verbose yes
    socket_path /tmp/foobar

    colors {
        background #ff0000
        statusline   #00ff00
        focused_background #cc0000
        focused_statusline #cccc00
        focused_separator  #0000cc

        focused_workspace   #4c7899 #285577 #ffffff
        active_workspace    #333333 #222222 #888888
        inactive_workspace  #333333 #222222 #888888
        urgent_workspace    #2f343a #900000 #ffffff
        binding_mode        #abc123 #123abc #ababab
    }

    # old syntax for compatibility with i3-gaps
    height 50
    # new syntax: top right bottom left
    #             y   width height x
    padding 4px 2px 4px 2px
}
EOT

$pid = launch_with_config($config);

$i3 = i3(get_socket_path(0));
$bars = $i3->get_bar_config()->recv;
is(@$bars, 1, 'one bar configured');

$bar_id = shift @$bars;

$bar_config = $i3->get_bar_config($bar_id)->recv;
is($bar_config->{status_command}, 'i3status --bar', 'status_command correct');
ok($bar_config->{verbose}, 'verbose on');
ok(!$bar_config->{workspace_buttons}, 'workspace buttons disabled');
is($bar_config->{workspace_min_width}, 30, 'workspace_min_width ok');
ok(!$bar_config->{binding_mode_indicator}, 'mode indicator disabled');
is($bar_config->{mode}, 'dock', 'dock mode');
is($bar_config->{position}, 'top', 'position top');
is_deeply($bar_config->{outputs}, [ 'HDMI1', 'HDMI2' ], 'outputs ok');
is_deeply($bar_config->{tray_outputs}, [ 'LVDS1', 'HDMI2' ], 'tray_output ok');
is($bar_config->{tray_padding}, 0, 'tray_padding ok');
is($bar_config->{font}, 'Terminus', 'font ok');
is($bar_config->{socket_path}, '/tmp/foobar', 'socket_path ok');
is($bar_config->{bar_height}, 50, 'bar_height ok');
is_deeply($bar_config->{padding},
	  {
	      x => 2,
	      y => 4,
	      width => 2,
	      height => 4,
	  }, 'padding ok');
is_deeply($bar_config->{colors},
    {
        background => '#ff0000',
        statusline => '#00ff00',
        focused_background => '#cc0000',
        focused_statusline=> '#cccc00',
        focused_separator => '#0000cc',
        focused_workspace_border => '#4c7899',
        focused_workspace_text => '#ffffff',
        focused_workspace_bg => '#285577',
        active_workspace_border => '#333333',
        active_workspace_text => '#888888',
        active_workspace_bg => '#222222',
        inactive_workspace_border => '#333333',
        inactive_workspace_text => '#888888',
        inactive_workspace_bg => '#222222',
        urgent_workspace_border => '#2f343a',
        urgent_workspace_text => '#ffffff',
        urgent_workspace_bg => '#900000',
        binding_mode_border => '#abc123',
        binding_mode_text => '#ababab',
        binding_mode_bg => '#123abc',
    }, 'colors ok');

exit_gracefully($pid);

#####################################################################
# validate one-value padding directive
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bar {
    # all padding is 25px
    padding 25px
}
EOT

$pid = launch_with_config($config);

$i3 = i3(get_socket_path(0));
$bars = $i3->get_bar_config()->recv;
is(@$bars, 1, 'one bar configured');

$bar_id = shift @$bars;

$bar_config = $i3->get_bar_config($bar_id)->recv;
is_deeply($bar_config->{padding},
	  {
	      x => 25,
	      y => 25,
	      width => 25,
	      height => 25,
	  }, 'padding ok');

exit_gracefully($pid);

#####################################################################
# validate two-value padding directive
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bar {
    # top and bottom padding is 25px, right and left padding is 50px
    padding 25px 50px
}
EOT

$pid = launch_with_config($config);

$i3 = i3(get_socket_path(0));
$bars = $i3->get_bar_config()->recv;
is(@$bars, 1, 'one bar configured');

$bar_id = shift @$bars;

$bar_config = $i3->get_bar_config($bar_id)->recv;
is_deeply($bar_config->{padding},
	  {
	      x => 50,
	      y => 25,
	      width => 50,
	      height => 25,
	  }, 'padding ok');

exit_gracefully($pid);

#####################################################################
# validate three-value padding directive
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bar {
    # top padding is 25px, right and left padding is 50px, bottom padding is 75px
    padding 25px 50px 75px
}
EOT

$pid = launch_with_config($config);

$i3 = i3(get_socket_path(0));
$bars = $i3->get_bar_config()->recv;
is(@$bars, 1, 'one bar configured');

$bar_id = shift @$bars;

$bar_config = $i3->get_bar_config($bar_id)->recv;
is_deeply($bar_config->{padding},
	  {
	      x => 50,
	      y => 25,
	      width => 50,
	      height => 75,
	  }, 'padding ok');

exit_gracefully($pid);

#####################################################################
# ensure that multiple bars get different IDs
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bar {
    # Start a default instance of i3bar which provides workspace buttons.
    # Additionally, i3status will provide a statusline.
    status_command i3status --bar

    output HDMI1
}

bar {
    output VGA1
}
EOT

$pid = launch_with_config($config);

$i3 = i3(get_socket_path(0));
$bars = $i3->get_bar_config()->recv;
is(@$bars, 2, 'two bars configured');
isnt($bars->[0], $bars->[1], 'bar IDs are different');

my $bar1_config = $i3->get_bar_config($bars->[0])->recv;
my $bar2_config = $i3->get_bar_config($bars->[1])->recv;

isnt($bar1_config->{outputs}, $bar2_config->{outputs}, 'outputs different');

exit_gracefully($pid);

#####################################################################
# make sure comments work properly
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bar {
    # Start a default instance of i3bar which provides workspace buttons.
    # Additionally, i3status will provide a statusline.
    status_command i3status --bar
    #status_command i3status --qux
#status_command i3status --qux

    output HDMI1
    colors {
        background #000000
        #background #ffffff
    }
}
EOT

$pid = launch_with_config($config);

$i3 = i3(get_socket_path(0));
$bars = $i3->get_bar_config()->recv;
$bar_id = shift @$bars;

$bar_config = $i3->get_bar_config($bar_id)->recv;
is($bar_config->{status_command}, 'i3status --bar', 'status_command correct');
is($bar_config->{colors}->{background}, '#000000', 'background color ok');

exit_gracefully($pid);

#####################################################################
# verify that the old syntax still works
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bar {
    # Start a default instance of i3bar which does not provide
    # workspace buttons.
    # Additionally, i3status will provide a statusline.
    status_command i3status --bar

    output HDMI1
    output HDMI2

    tray_output LVDS1
    tray_output HDMI2
    position top
    mode dock
    font Terminus
    workspace_buttons no
    binding_mode_indicator yes
    verbose yes
    socket_path /tmp/foobar

    colors {
        background #ff0000
        statusline   #00ff00

        focused_workspace   #ffffff #285577
        active_workspace    #888888 #222222
        inactive_workspace  #888888 #222222
        urgent_workspace    #ffffff #900000
    }
}
EOT

$pid = launch_with_config($config);

$i3 = i3(get_socket_path(0));
$bars = $i3->get_bar_config()->recv;
is(@$bars, 1, 'one bar configured');

$bar_id = shift @$bars;

cmd 'nop yeah';
$bar_config = $i3->get_bar_config($bar_id)->recv;
is($bar_config->{status_command}, 'i3status --bar', 'status_command correct');
ok($bar_config->{verbose}, 'verbose on');
ok(!$bar_config->{workspace_buttons}, 'workspace buttons disabled');
ok($bar_config->{binding_mode_indicator}, 'mode indicator enabled');
is($bar_config->{mode}, 'dock', 'dock mode');
is($bar_config->{position}, 'top', 'position top');
is_deeply($bar_config->{outputs}, [ 'HDMI1', 'HDMI2' ], 'outputs ok');
is_deeply($bar_config->{tray_outputs}, [ 'LVDS1', 'HDMI2' ], 'tray_output ok');
is($bar_config->{font}, 'Terminus', 'font ok');
is($bar_config->{socket_path}, '/tmp/foobar', 'socket_path ok');
is_deeply($bar_config->{colors},
    {
        background => '#ff0000',
        statusline => '#00ff00',
        focused_workspace_text => '#ffffff',
        focused_workspace_bg => '#285577',
        active_workspace_text => '#888888',
        active_workspace_bg => '#222222',
        inactive_workspace_text => '#888888',
        inactive_workspace_bg => '#222222',
        urgent_workspace_text => '#ffffff',
        urgent_workspace_bg => '#900000',
    }, '(old) colors ok');

exit_gracefully($pid);


done_testing;
