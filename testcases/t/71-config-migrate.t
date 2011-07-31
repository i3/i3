#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
# Tests if i3-migrate-config-to-v4.pl correctly migrates all config file
# directives and commands
#
use i3test;
use Cwd qw(abs_path);
use Proc::Background;
use File::Temp qw(tempfile tempdir);
use POSIX qw(getuid);
use Data::Dumper;
use v5.10;

# reads in a whole file
sub slurp {
    open my $fh, '<', shift;
    local $/;
    <$fh>;
}

sub migrate_config {
    my ($config) = @_;

    my ($fh, $tmpfile) = tempfile();
    print $fh $config;
    close($fh);

    my $cmd = "sh -c 'exec " . abs_path("../i3-migrate-config-to-v4.pl") . " --v3 <$tmpfile'";
    return [ split /\n/, qx($cmd) ];
}

sub line_exists {
    my ($lines, $pattern) = @_;

    for my $line (@$lines) {
        return 1 if $line =~ $pattern;
    }

    return 0
}

#####################################################################
# check that some directives remain untouched
#####################################################################

my $input = <<EOT;
    font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $output = migrate_config($input);
ok(line_exists($output, qr|font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1|), 'font directive unchanged');

$input = <<EOT;
    floating_Modifier Mod1
    focus_follows_mouse true
    ipc-socket /tmp/i3-ipc.sock
    ipc_socket /tmp/i3-ipc.sock
    exec /usr/bin/i3
    set stuff Mod1
    assign "XTerm" → 3
    assign "XTerm" → ~5
    client.focused #2F343A #900000 #FFFFFF
    client.focused_inactive #FF0000 #FF0000 #FF0000
    client.unfocused #00FF00 #00FF00 #00FF00
    client.urgent #0000FF #0000FF #0000FF
    client.background #000000
EOT

$output = migrate_config($input);
ok(line_exists($output, qr|^floating_Modifier Mod1$|), 'floating_modifier unchanged');
ok(line_exists($output, qr|^focus_follows_mouse true$|), 'focus_follows_mouse unchanged');
ok(line_exists($output, qr|^ipc-socket /tmp/i3-ipc.sock$|), 'ipc-socket unchanged');
ok(line_exists($output, qr|^ipc_socket /tmp/i3-ipc.sock$|), 'ipc_socket unchanged');
ok(line_exists($output, qr|^exec /usr/bin/i3|), 'exec unchanged');
ok(line_exists($output, qr|^set stuff Mod1|), 'set unchanged');
ok(line_exists($output, qr|^assign "XTerm" → 3|), 'assign unchanged');
ok(line_exists($output, qr|^assign "XTerm" → ~5|), 'assign unchanged');
ok(line_exists($output, qr|^client\.focused #2F343A #900000 #FFFFFF$|), 'client.focused unchanged');
ok(line_exists($output, qr|^client\.focused_inactive #FF0000 #FF0000 #FF0000$|), 'client.focused_inactive unchanged');
ok(line_exists($output, qr|^client\.unfocused #00FF00 #00FF00 #00FF00$|), 'client.unfocused unchanged');
ok(line_exists($output, qr|^client\.urgent #0000FF #0000FF #0000FF$|), 'client.urgent unchanged');
ok(line_exists($output, qr|^client\.background #000000$|), 'client.background unchanged');

#####################################################################
# check whether the bar colors get removed properly
#####################################################################

$input = <<EOT;
    bar.focused #FFFF00 #FFFF00 #FFFF00
    bar.unfocused #FFFF00 #FFFF00 #FFFF00
    bar.urgent #FFFF00 #FFFF00 #FFFF00
EOT

$output = migrate_config($input);
ok(!line_exists($output, qr|^bar\.|), 'no bar. lines');
ok(line_exists($output, qr|^#.*REMOVED bar|), 'note bar. removed');


#####################################################################
# check whether the other directives get converted correctly
#####################################################################

$input = <<EOT;
    new_container stacking
    workspace_bar no
    new_window bb
EOT

$output = migrate_config($input);
ok(line_exists($output, qr|^workspace_layout stacking$|), 'new_container changed');
ok(line_exists($output, qr|REMOVED workspace_bar|), 'workspace_bar removed');
ok(!line_exists($output, qr|^workspace_bar|), 'no workspace_bar in the output');
ok(line_exists($output, qr|^new_window none$|), 'new_window changed');

#####################################################################
# check whether new_window's parameters get changed correctly
#####################################################################

$output = migrate_config('new_window bb');
ok(line_exists($output, qr|^new_window none$|), 'new_window bb changed');

$output = migrate_config('new_window bn');
ok(line_exists($output, qr|^new_window normal$|), 'new_window bn changed');

$output = migrate_config('new_window bp');
ok(line_exists($output, qr|^new_window 1pixel$|), 'new_window bp changed');

#####################################################################
# check that some commands remain untouched
#####################################################################

$input = <<EOT;
    bindsym Mod1+s exec /usr/bin/urxvt
    bindsym Mod1+s mark foo
    bindsym Mod1+s restart
    bindsym Mod1+s reload
    bindsym Mod1+s exit
    bindsym Mod1+s stack-limit cols 2
    bindsym Mod1+s stack-limit rows 3
    bind Mod1+c exec /usr/bin/urxvt
    mode "asdf" {
        bind 36 mode default
    }
EOT

$output = migrate_config($input);
ok(line_exists($output, qr|^bindsym Mod1\+s exec /usr/bin/urxvt$|), 'exec unchanged');
ok(line_exists($output, qr|^bindsym Mod1\+s mark foo$|), 'mark unchanged');
ok(line_exists($output, qr|^bindsym Mod1\+s restart$|), 'restart unchanged');
ok(line_exists($output, qr|^bindsym Mod1\+s reload$|), 'reload unchanged');
ok(line_exists($output, qr|^bindsym Mod1\+s exit$|), 'exit unchanged');
ok(line_exists($output, qr|^bindsym Mod1\+s stack-limit cols 2$|), 'stack-limit unchanged');
ok(line_exists($output, qr|^bindsym Mod1\+s stack-limit rows 3$|), 'stack-limit unchanged');
ok(line_exists($output, qr|^bindcode Mod1\+c exec /usr/bin/urxvt$|), 'bind changed to bindcode');
ok(line_exists($output, qr|^mode "asdf" {$|), 'mode asdf unchanged');
ok(line_exists($output, qr|^bindcode 36 mode \"default\"$|), 'mode default unchanged');
ok(line_exists($output, qr|^}$|), 'closing mode bracket still there');

#####################################################################
# check the simple command replacements
#####################################################################

$input = <<EOT;
    bindsym Mod1+s s
    bindsym Mod1+s d
    bindsym Mod1+s T

    bindsym Mod1+s f
    bindsym Mod1+s fg

    bindsym Mod1+s t

    bindsym Mod1+s h
    bindsym Mod1+s j
    bindsym Mod1+s k
    bindsym Mod1+s l

    bindsym Mod1+s mh
    bindsym Mod1+s mj
    bindsym Mod1+s mk
    bindsym Mod1+s ml

    bindsym Mod1+s bn
    bindsym Mod1+s bp
    bindsym Mod1+s bb
    bindsym Mod1+s bt

    bindsym Mod1+j wch
    bindsym Mod1+j wcml

    bindsym Mod1+k kill

    bindsym Mod1+n nw
    bindsym Mod1+p pw
EOT

$output = migrate_config($input);
ok(line_exists($output, qr|^bindsym Mod1\+s layout stacking$|), 's replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s layout default$|), 'd replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s layout tabbed$|), 'T replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s fullscreen$|), 'f replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s fullscreen global$|), 'fg replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s floating toggle$|), 't replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s focus left$|), 'h replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s focus down$|), 'j replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s focus up$|), 'k replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s focus right$|), 'l replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s move left$|), 'mh replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s move down$|), 'mj replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s move up$|), 'mk replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s move right$|), 'ml replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s border normal$|), 'bn replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s border 1pixel$|), 'bp replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s border none$|), 'bb replaced');
ok(line_exists($output, qr|^bindsym Mod1\+s border toggle$|), 'bt replaced');
ok(line_exists($output, qr|^bindsym Mod1\+j focus parent; focus left$|), 'with container replaced with focus parent; focus left');
ok(line_exists($output, qr|^bindsym Mod1\+j focus parent; move right$|), 'with container replaced with focus parent; move right');
ok(line_exists($output, qr|^bindsym Mod1\+k kill$|), 'kill unchanged');
ok(line_exists($output, qr|^bindsym Mod1\+n workspace next$|), 'nw replaced');
ok(line_exists($output, qr|^bindsym Mod1\+p workspace prev$|), 'pw replaced');

#####################################################################
# check more advanced replacements
#####################################################################

$input = <<EOT;
    bindsym Mod1+s goto foo
EOT

$output = migrate_config($input);
ok(line_exists($output, qr|^bindsym Mod1\+s \[con_mark="foo"\] focus$|), 'goto replaced');

#####################################################################
# check whether focus's parameters get changed correctly
#####################################################################

$output = migrate_config('bindsym Mod1+f focus 3');
ok(line_exists($output, qr|^#.*focus.*obsolete.*focus 3$|), 'focus [number] gone');

$output = migrate_config('bindsym Mod1+f focus floating');
ok(line_exists($output, qr|^bindsym Mod1\+f focus floating$|), 'focus floating unchanged');

$output = migrate_config('bindsym Mod1+f focus tiling');
ok(line_exists($output, qr|^bindsym Mod1\+f focus tiling$|), 'focus tiling unchanged');

$output = migrate_config('bindsym Mod1+f focus ft');
ok(line_exists($output, qr|^bindsym Mod1\+f focus mode_toggle$|), 'focus ft changed');

#####################################################################
# check whether resize's parameters get changed correctly
#####################################################################

$output = migrate_config('bindsym Mod1+f resize left +10');
ok(line_exists($output, qr|^bindsym Mod1\+f resize grow left 10 px$|), 'resize left changed');

$output = migrate_config('bindsym Mod1+f resize top -20');
ok(line_exists($output, qr|^bindsym Mod1\+f resize shrink top 20 px$|), 'resize top changed');

$output = migrate_config('bindsym Mod1+f resize right -20');
ok(line_exists($output, qr|^bindsym Mod1\+f resize shrink right 20 px$|), 'resize right changed');

$output = migrate_config('bindsym Mod1+f resize bottom +23');
ok(line_exists($output, qr|^bindsym Mod1\+f resize grow bottom 23 px$|), 'resize bottom changed');

#####################################################################
# check whether jump's parameters get changed correctly
#####################################################################

$output = migrate_config('bindsym Mod1+f jump 3');
ok(line_exists($output, qr|^#.*obsolete.*jump 3$|), 'jump to workspace removed');

$output = migrate_config('bindsym Mod1+f jump 3 4 5');
ok(line_exists($output, qr|^#.*obsolete.*jump 3 4 5$|), 'jump to workspace + col/row removed');

$output = migrate_config('bindsym Mod1+f jump "XTerm"');
ok(line_exists($output, qr|^bindsym Mod1\+f \[class="XTerm"\] focus$|), 'jump changed');

$output = migrate_config('bindsym Mod1+f jump "XTerm/irssi"');
ok(line_exists($output, qr|^bindsym Mod1\+f \[class="XTerm" title="irssi"\] focus$|), 'jump changed');

#####################################################################
# check whether workspace commands are handled correctly
#####################################################################

$output = migrate_config('workspace 3 output VGA-1');
ok(line_exists($output, qr|^workspace 3 output VGA-1$|), 'workspace assignment unchanged');

$output = migrate_config('workspace 3 work');
ok(!line_exists($output, qr|^workspace|), 'workspace name not present');
ok(line_exists($output, qr|#.*workspace name.*bindings|), 'note present');

$input = <<EOT;
    workspace 3 work
    bindsym Mod1+3 3
EOT
$output = migrate_config($input);
ok(!line_exists($output, qr|^workspace|), 'workspace name not present');
ok(line_exists($output, qr|^bindsym Mod1\+3 workspace work|), 'named workspace in bindings');

# The same, but in reverse order
$input = <<EOT;
    bindsym Mod1+3 3
    workspace 3 work
EOT
$output = migrate_config($input);
ok(!line_exists($output, qr|^workspace|), 'workspace name not present');
ok(line_exists($output, qr|^bindsym Mod1\+3 workspace work|), 'named workspace in bindings');

$output = migrate_config('bindsym Mod1+3 3');
ok(line_exists($output, qr|^bindsym Mod1\+3 workspace 3|), 'workspace changed');

$output = migrate_config('bindsym Mod1+3 m3');
ok(line_exists($output, qr|^bindsym Mod1\+3 move workspace 3|), 'move workspace changed');

$input = <<EOT;
    workspace 3 work
    bindsym Mod1+3 m3
EOT
$output = migrate_config($input);
ok(!line_exists($output, qr|^workspace|), 'workspace name not present');
ok(line_exists($output, qr|^bindsym Mod1\+3 move workspace work|), 'move to named workspace in bindings');

#####################################################################
# check whether an i3bar call is added if the workspace bar bar was enabled
#####################################################################

$output = migrate_config('');
ok(line_exists($output, qr|i3bar|), 'i3bar added');

$output = migrate_config('workspace_bar enable');
ok(line_exists($output, qr|i3bar|), 'i3bar added');

$output = migrate_config('workspace_bar no');
ok(!line_exists($output, qr|i3bar|), 'no i3bar added');

#####################################################################
# check whether the mode command gets quotes
#####################################################################

$output = migrate_config('bindsym Mod1+m mode foobar');
ok(line_exists($output, qr|^bindsym Mod1\+m mode "foobar"|), 'mode got quotes');

done_testing();
