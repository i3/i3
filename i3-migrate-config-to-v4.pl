#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
#
# script to migrate an old config file (i3 < 4.0) to the new format (>= 4.0)
# this script only uses modules which come with perl 5.10
#
# reads an i3 v3 config from stdin and spits out a v4 config on stdout
# exit codes:
#     0 = the input was a v3 config
#     1 = the input was already a v4 config
#         (the config is printed to stdout nevertheless)
#
# © 2011 Michael Stapelberg and contributors, see LICENSE

use strict;
use warnings;
use Getopt::Long;
use v5.10;

# is this a version 3 config file? disables auto-detection
my $v3 = 0;
my $result = GetOptions('v3' => \$v3);

# reads stdin
sub slurp {
    local $/;
    <>;
}

my @unchanged = qw(
    font
    set
    mode
    exec
    assign
    floating_modifier
    focus_follows_mouse
    ipc-socket
    ipc_socket
    client.focused
    client.focused_inactive
    client.unfocused
    client.urgent
    client.background
);

my %workspace_names;
my $workspace_bar = 1;

my $input = slurp();
my @lines = split /\n/, $input;

# remove whitespaces in the beginning of lines
@lines = map { s/^[ \t]*//g; $_ } @lines;

# Try to auto-detect if this is a v3 config file.
sub need_to_convert {
    # If the user passed --v3, we need to convert in any case
    return 1 if $v3;

    for my $line (@lines) {
        # only v4 configfiles can use bindcode or workspace_layout
        return 0 if $line =~ /^bindcode/ || $line =~ /^workspace_layout/;

        # have a look at bindings
        next unless $line =~ /^bind/;

        my ($statement, $key, $command) = ($line =~ /([a-zA-Z_-]+)[ \t]+([^ \t]+)[ \t]+(.*)/);
        return 0 if $command =~ /^layout/ ||
                    $command =~ /^floating/ ||
                    $command =~ /^workspace/ ||
                    $command =~ /^focus (left|right|up|down)/ ||
                    $command =~ /^border (normal|1pixel|borderless)/;
    }

    return 1;
}

if (!need_to_convert()) {
    # If this is already a v4 config file, we will spit out the lines
    # and exit with return code 1
    print $input;
    exit 1;
}

for my $line (@lines) {
    # directly use comments and empty lines
    if ($line =~ /^#/ || $line =~ /^$/) {
        print "$line\n";
        next;
    }

    my ($statement, $parameters) = ($line =~ /([a-zA-Z._-]+)(.*)/);

    # directly use lines which have not changed between 3.x and 4.x
    if (!defined($statement) || (lc $statement ~~ @unchanged)) {
        print "$line\n";
        next;
    }

    # new_container changed only the statement name to workspace_layout
    if ($statement eq 'new_container') {
        # TODO: new_container stack-limit
        print "workspace_layout$parameters\n";
        next;
    }

    # workspace_bar is gone, you should use i3bar now
    if ($statement eq 'workspace_bar') {
        $workspace_bar = ($parameters =~ /[ \t+](yes|true|on|enable|active)/);
        print "# XXX: REMOVED workspace_bar line. There is no internal workspace bar in v4.\n";
        next;
    }

    # new_window changed the parameters from bb to borderless etc.
    if ($statement eq 'new_window') {
        if ($parameters =~ /bb/) {
            print "new_window borderless\n";
        } elsif ($parameters =~ /bp/) {
            print "new_window 1pixel\n";
        } elsif ($parameters =~ /bn/) {
            print "new_window normal\n";
        } else {
            print "# XXX: Invalid parameter for new_window, not touching line:\n";
            print "$line\n";
        }
        next;
    }

    # bar colors are obsolete, need to be configured in i3bar
    if ($statement =~ /^bar\./) {
        print "# XXX: REMOVED $statement, configure i3bar instead.\n";
        print "# Old line: $line\n";
        next;
    }

    # one form of this is still ok (workspace assignments), the other (named workspaces) isn’t
    if ($statement eq 'workspace') {
        my ($number, $params) = ($parameters =~ /[ \t]+([0-9]+) (.+)/);
        if ($params =~ /^output/) {
            print "$line\n";
            next;
        } else {
            print "# XXX: workspace name will end up in the bindings, see below\n";
            $workspace_names{$number} = $params;
            next;
        }
    }

    if ($statement eq 'bind' || $statement eq 'bindsym') {
        convert_command($line);
        next;
    }

    print "# XXX: migration script could not handle line: $line\n";
}

#
# Converts a command (after bind/bindsym)
#
sub convert_command {
    my ($line) = @_;

    my @unchanged_cmds = qw(
        exec
        mode
        mark
        kill
        restart
        reload
        exit
        stack-limit
    );

    my ($statement, $key, $command) = ($line =~ /([a-zA-Z_-]+)[ \t]+([^ \t]+)[ \t]+(.*)/);

    # turn 'bind' to 'bindcode'
    $statement = 'bindcode' if $statement eq 'bind';

    # check if it’s one of the unchanged commands
    my ($cmd) = ($command =~ /([a-zA-Z_-]+)/);
    if ($cmd ~~ @unchanged_cmds) {
        print "$statement $key $command\n";
        return;
    }

    # simple replacements
    my @replace = (
        qr/^s/ => 'layout stacking',
        qr/^d/ => 'layout default',
        qr/^T/ => 'layout tabbed',
        qr/^f($|[^go])/ => 'fullscreen',
        qr/^fg/ => 'fullscreen global',
        qr/^t/ => 'focus mode_toggle',
        qr/^h/ => 'focus left',
        qr/^j($|[^u])/ => 'focus down',
        qr/^k/ => 'focus up',
        qr/^l/ => 'focus right',
        qr/^mh/ => 'move left',
        qr/^mj/ => 'move down',
        qr/^mk/ => 'move up',
        qr/^ml/ => 'move right',
        qr/^bn/ => 'border normal',
        qr/^bp/ => 'border 1pixel',
        qr/^bb/ => 'border borderless',
        qr/^pw/ => 'workspace prev',
        qr/^nw/ => 'workspace next',
    );

    my $replaced = 0;
    for (my $c = 0; $c < @replace; $c += 2) {
        if ($command =~ $replace[$c]) {
            $command = $replace[$c+1];
            $replaced = 1;
            last;
        }
    }

    # goto command is now obsolete due to criteria + focus command
    if ($command =~ /^goto/) {
        my ($mark) = ($command =~ /^goto (.*)/);
        print qq|$statement $key [con_mark="$mark"] focus\n|;
        return;
    }

    # the jump command is also obsolete due to criteria + focus
    if ($command =~ /^jump/) {
        my ($params) = ($command =~ /^jump (.*)/);
        if ($params =~ /^"/) {
            # jump ["]window class[/window title]["]
            ($params) = ($params =~ /^"([^"]+)"/);

            # check if a window title was specified
            if ($params =~ m,/,) {
                my ($class, $title) = ($params =~ m,([^/]+)/(.+),);
                print qq|$statement $key [class="$class" title="$title"] focus\n|;
            } else {
                print qq|$statement $key [class="$params"] focus\n|;
            }
            return;
        } else {
            # jump <workspace> [ column row ]
            print "# XXX: jump workspace is obsolete in 4.x: $line\n";
            return;
        }
    }

    if (!$replaced && $command =~ /^focus/) {
        my ($what) = ($command =~ /^focus (.*)/);
        $what =~ s/[ \t]//g;
        if ($what eq 'ft') {
            $what = 'mode_toggle';
        } elsif ($what eq 'floating' || $what eq 'tiling') {
            # those are unchanged
        } else {
            print "# XXX: focus <number> is obsolete in 4.x: $line\n";
            return;
        }
        print qq|$statement $key focus $what\n|;
        return;
    }

    # the parameters of the resize command have changed
    if ($command =~ /^resize/) {
        # OLD: resize <left|right|top|bottom> [+|-]<pixels>\n")
        # NEW: resize <grow|shrink> <direction> [<px> px] [or <ppt> ppt]
        my ($direction, $sign, $px) = ($command =~ /^resize (left|right|top|bottom) ([+-])([0-9]+)/);
        my $cmd = 'resize ';
        $cmd .= ($sign eq '+' ? 'grow' : 'shrink') . ' ';
        $cmd .= "$direction ";
        $cmd .= "$px px";
        print qq|$statement $key $cmd\n|;
        return;
    }

    # switch workspace
    if ($command =~ /^[0-9]+/) {
        my ($number) = ($command =~ /^([0-9]+)/);
        if (exists $workspace_names{$number}) {
            print qq|$statement $key workspace $workspace_names{$number}\n|;
            return;
        } else {
            print qq|$statement $key workspace $number\n|;
            return;
        }
    }

    # move to workspace
    if ($command =~ /^m[0-9]+/) {
        my ($number) = ($command =~ /^m([0-9]+)/);
        if (exists $workspace_names{$number}) {
            print qq|$statement $key move workspace $workspace_names{$number}\n|;
            return;
        } else {
            print qq|$statement $key move workspace $number\n|;
            return;
        }
    }

    # With Container-commands are now obsolete due to different abstraction
    if ($command =~ /^wc/) {
        print "# XXX: 'with container' commands are obsolete in 4.x: $line\n";
        return;
    }

    if ($replaced) {
        print "$statement $key $command\n";
    } else {
        print "# XXX: migration script could not handle command: $line\n";
    }
}


# add an i3bar invocation automatically if no 'workspace_bar no' was found
if ($workspace_bar) {
    print "\n";
    print "# XXX: Automatically added a call to i3bar to provide a workspace bar\n";
    print "exec i3status | i3bar -d\n";
}
