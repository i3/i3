#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
# © 2012 Michael Stapelberg
# Licensed under BSD license, see https://github.com/i3/i3/blob/next/LICENSE
#
# Append this line to your i3 config file:
#     exec_always ~/per-workspace-layout.pl
#
# Then, change the %layouts hash like you want your workspaces to be set up.
# This script requires i3 >= v4.4 for the extended workspace event.

use strict;
use warnings;
use AnyEvent;
use AnyEvent::I3;
use v5.10;
use utf8;

my %layouts = (
    '4' => 'tabbed',
    '5' => 'stacked',
);

my %new_ws;

my $i3 = i3();

die "Could not connect to i3: $!" unless $i3->connect->recv();

die "Could not subscribe to the workspace event: $!" unless
    $i3->subscribe({
        workspace => sub {
            my ($msg) = @_;
            return unless $msg->{change} eq 'focus' or $msg->{change} eq 'init';
            die "Your version of i3 is too old. You need >= v4.4"
                unless exists($msg->{current});
            my $ws = $msg->{current};

            my $name = $ws->{name};
           
            # if the workspace is created, store it in $new_ws so we can set
            # the layout on the first focus
            if($msg->{change} eq 'init') {
                return unless exists $layouts{$name};
                $new_ws{$name} = 1 unless exists $new_ws{$name};
                my $nodes = scalar @{$ws->{nodes}} == 0;
                return
            }

            my $con_id = $ws->{id};

            return unless exists $new_ws{$name};

            delete($new_ws{$name});
            $i3->command(qq|[con_id="$con_id"] layout | . $layouts{$name});
        },
        _error => sub {
            my ($msg) = @_;
            say "AnyEvent::I3 error: $msg";
            say "Exiting.";
            exit 1;
        },
    })->recv->{success};

# Run forever.
AnyEvent->condvar->recv
