package AnyEvent::I3;
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use JSON::XS;
use AnyEvent::Handle;
use AnyEvent::Socket;
use AnyEvent;
use Encode;
use Scalar::Util qw(tainted);
use Carp;

=head1 NAME

AnyEvent::I3 - communicate with the i3 window manager

=cut

our $VERSION = '0.18';

=head1 VERSION

Version 0.18

=head1 SYNOPSIS

This module connects to the i3 window manager using the UNIX socket based
IPC interface it provides (if enabled in the configuration file). You can
then subscribe to events or send messages and receive their replies.

    use AnyEvent::I3 qw(:all);

    my $i3 = i3();

    $i3->connect->recv or die "Error connecting";
    say "Connected to i3";

    my $workspaces = $i3->message(TYPE_GET_WORKSPACES)->recv;
    say "Currently, you use " . @{$workspaces} . " workspaces";

...or, using the sugar methods:

    use AnyEvent::I3;

    my $workspaces = i3->get_workspaces->recv;
    say "Currently, you use " . @{$workspaces} . " workspaces";

A somewhat more involved example which dumps the i3 layout tree whenever there
is a workspace event:

    use Data::Dumper;
    use AnyEvent;
    use AnyEvent::I3;

    my $i3 = i3();

    $i3->connect->recv or die "Error connecting to i3";

    $i3->subscribe({
        workspace => sub {
            $i3->get_tree->cb(sub {
                my ($tree) = @_;
                say "tree: " . Dumper($tree);
            });
        }
    })->recv->{success} or die "Error subscribing to events";

    AE::cv->recv

=head1 EXPORT

=head2 $i3 = i3([ $path ]);

Creates a new C<AnyEvent::I3> object and returns it.

C<path> is an optional path of the UNIX socket to connect to. It is strongly
advised to NOT specify this unless you're absolutely sure you need it.
C<AnyEvent::I3> will automatically figure it out by querying the running i3
instance on the current DISPLAY which is almost always what you want.

=head1 SUBROUTINES/METHODS

=cut

use Exporter qw(import);
use base 'Exporter';

our @EXPORT = qw(i3);

use constant TYPE_RUN_COMMAND => 0;
use constant TYPE_COMMAND => 0;
use constant TYPE_GET_WORKSPACES => 1;
use constant TYPE_SUBSCRIBE => 2;
use constant TYPE_GET_OUTPUTS => 3;
use constant TYPE_GET_TREE => 4;
use constant TYPE_GET_MARKS => 5;
use constant TYPE_GET_BAR_CONFIG => 6;
use constant TYPE_GET_VERSION => 7;
use constant TYPE_GET_BINDING_MODES => 8;
use constant TYPE_GET_CONFIG => 9;

our %EXPORT_TAGS = ( 'all' => [
    qw(i3 TYPE_RUN_COMMAND TYPE_COMMAND TYPE_GET_WORKSPACES TYPE_SUBSCRIBE TYPE_GET_OUTPUTS
       TYPE_GET_TREE TYPE_GET_MARKS TYPE_GET_BAR_CONFIG TYPE_GET_VERSION
       TYPE_GET_BINDING_MODES TYPE_GET_CONFIG)
] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{all} } );

my $magic = "i3-ipc";

# TODO: auto-generate this from the header file? (i3/ipc.h)
my $event_mask = (1 << 31);
my %events = (
    workspace => ($event_mask | 0),
    output => ($event_mask | 1),
    mode => ($event_mask | 2),
    window => ($event_mask | 3),
    barconfig_update => ($event_mask | 4),
    binding => ($event_mask | 5),
    shutdown => ($event_mask | 6),
    _error => 0xFFFFFFFF,
);

sub i3 {
    AnyEvent::I3->new(@_)
}

# Calls i3, even when running in taint mode.
sub _call_i3 {
    my ($args) = @_;

    my $path_tainted = tainted($ENV{PATH});
    # This effectively circumvents taint mode checking for $ENV{PATH}. We
    # do this because users might specify PATH explicitly to call i3 in a
    # custom location (think ~/.bin/).
    (local $ENV{PATH}) = ($ENV{PATH} =~ /(.*)/);

    # In taint mode, we also need to remove all relative directories from
    # PATH (like . or ../bin). We only do this in taint mode and warn the
    # user, since this might break a real-world use case for some people.
    if ($path_tainted) {
        my @dirs = split /:/, $ENV{PATH};
        my @filtered = grep !/^\./, @dirs;
        if (scalar @dirs != scalar @filtered) {
            $ENV{PATH} = join ':', @filtered;
            warn qq|Removed relative directories from PATH because you | .
                 qq|are running Perl with taint mode enabled. Remove -T | .
                 qq|to be able to use relative directories in PATH. | .
                 qq|New PATH is "$ENV{PATH}"|;
        }
    }
    # Otherwise the qx() operator wont work:
    delete @ENV{'IFS', 'CDPATH', 'ENV', 'BASH_ENV'};
    chomp(my $result = qx(i3 $args));
    # Circumventing taint mode again: the socket can be anywhere on the
    # system and that’s okay.
    if ($result =~ /^([^\0]+)$/) {
        return $1;
    }

    warn "Calling i3 $args failed. Is DISPLAY set and is i3 in your PATH?";
    return undef;
}

=head2 $i3 = AnyEvent::I3->new([ $path ])

Creates a new C<AnyEvent::I3> object and returns it.

C<path> is an optional path of the UNIX socket to connect to. It is strongly
advised to NOT specify this unless you're absolutely sure you need it.
C<AnyEvent::I3> will automatically figure it out by querying the running i3
instance on the current DISPLAY which is almost always what you want.

=cut
sub new {
    my ($class, $path) = @_;

    $path = _call_i3('--get-socketpath') unless $path;

    # This is the old default path (v3.*). This fallback line can be removed in
    # a year from now. -- Michael, 2012-07-09
    $path ||= '~/.i3/ipc.sock';

    # Check if we need to resolve ~
    if ($path =~ /~/) {
        # We use getpwuid() instead of $ENV{HOME} because the latter is tainted
        # and thus produces warnings when running tests with perl -T
        my $home = (getpwuid($<))[7];
        confess "Could not get home directory" unless $home and -d $home;
        $path =~ s/~/$home/g;
    }

    bless { path => $path } => $class;
}

=head2 $i3->connect

Establishes the connection to i3. Returns an C<AnyEvent::CondVar> which will
be triggered with a boolean (true if the connection was established) as soon as
the connection has been established.

    if ($i3->connect->recv) {
        say "Connected to i3";
    }

=cut
sub connect {
    my ($self) = @_;
    my $cv = AnyEvent->condvar;

    tcp_connect "unix/", $self->{path}, sub {
        my ($fh) = @_;

        return $cv->send(0) unless $fh;

        $self->{ipchdl} = AnyEvent::Handle->new(
            fh => $fh,
            on_read => sub { my ($hdl) = @_; $self->_data_available($hdl) },
            on_error => sub {
                my ($hdl, $fatal, $msg) = @_;
                delete $self->{ipchdl};
                $hdl->destroy;

                my $cb = $self->{callbacks};

                # Trigger all one-time callbacks with undef
                for my $type (keys %{$cb}) {
                    next if ($type & $event_mask) == $event_mask;
                    $cb->{$type}->();
                    delete $cb->{$type};
                }

                # Trigger _error callback, if set
                my $type = $events{_error};
                return unless defined($cb->{$type});
                $cb->{$type}->($msg);
            }
        );

        $cv->send(1)
    };

    $cv
}

sub _data_available {
    my ($self, $hdl) = @_;

    $hdl->unshift_read(
        chunk => length($magic) + 4 + 4,
        sub {
            my $header = $_[1];
            # Unpack message length and read the payload
            my ($len, $type) = unpack("LL", substr($header, length($magic)));
            $hdl->unshift_read(
                chunk => $len,
                sub { $self->_handle_i3_message($type, $_[1]) }
            );
        }
    );
}

sub _handle_i3_message {
    my ($self, $type, $payload) = @_;

    return unless defined($self->{callbacks}->{$type});

    my $cb = $self->{callbacks}->{$type};
    $cb->(decode_json $payload);

    return if ($type & $event_mask) == $event_mask;

    # If this was a one-time callback, we delete it
    # (when connection is lost, all one-time callbacks get triggered)
    delete $self->{callbacks}->{$type};
}

=head2 $i3->subscribe(\%callbacks)

Subscribes to the given event types. This function awaits a hashref with the
key being the name of the event and the value being a callback.

    my %callbacks = (
        workspace => sub { say "Workspaces changed" }
    );

    if ($i3->subscribe(\%callbacks)->recv->{success}) {
        say "Successfully subscribed";
    }

The special callback with name C<_error> is called when the connection to i3
is killed (because of a crash, exit or restart of i3 most likely). You can
use it to print an appropriate message and exit cleanly or to try to reconnect.

    my %callbacks = (
        _error => sub {
            my ($msg) = @_;
            say "I am sorry. I am so sorry: $msg";
            exit 1;
        }
    );

    $i3->subscribe(\%callbacks)->recv;

=cut
sub subscribe {
    my ($self, $callbacks) = @_;

    # Register callbacks for each message type
    for my $key (keys %{$callbacks}) {
        my $type = $events{$key};
        $self->{callbacks}->{$type} = $callbacks->{$key};
    }

    $self->message(TYPE_SUBSCRIBE, [ keys %{$callbacks} ])
}

=head2 $i3->message($type, $content)

Sends a message of the specified C<type> to i3, possibly containing the data
structure C<content> (or C<content>, encoded as utf8, if C<content> is a
scalar), if specified.

    my $reply = $i3->message(TYPE_RUN_COMMAND, "reload")->recv;
    if ($reply->{success}) {
        say "Configuration successfully reloaded";
    }

=cut
sub message {
    my ($self, $type, $content) = @_;

    confess "No message type specified" unless defined($type);

    confess "No connection to i3" unless defined($self->{ipchdl});

    my $payload = "";
    if ($content) {
        if (not ref($content)) {
            # Convert from Perl’s internal encoding to UTF8 octets
            $payload = encode_utf8($content);
        } else {
            $payload = encode_json $content;
        }
    }
    my $message = $magic . pack("LL", length($payload), $type) . $payload;
    $self->{ipchdl}->push_write($message);

    my $cv = AnyEvent->condvar;

    # We don’t preserve the old callback as it makes no sense to
    # have a callback on message reply types (only on events)
    $self->{callbacks}->{$type} =
        sub {
            my ($reply) = @_;
            $cv->send($reply);
            undef $self->{callbacks}->{$type};
        };

    $cv
}

=head1 SUGAR METHODS

These methods intend to make your scripts as beautiful as possible. All of
them automatically establish a connection to i3 blockingly (if it does not
already exist).

=cut

sub _ensure_connection {
    my ($self) = @_;

    return if defined($self->{ipchdl});

    $self->connect->recv or confess "Unable to connect to i3 (socket path " . $self->{path} . ")";
}

=head2 get_workspaces

Gets the current workspaces from i3.

    my $ws = i3->get_workspaces->recv;
    say Dumper($ws);

=cut
sub get_workspaces {
    my ($self) = @_;

    $self->_ensure_connection;

    $self->message(TYPE_GET_WORKSPACES)
}

=head2 get_outputs

Gets the current outputs from i3.

    my $outs = i3->get_outputs->recv;
    say Dumper($outs);

=cut
sub get_outputs {
    my ($self) = @_;

    $self->_ensure_connection;

    $self->message(TYPE_GET_OUTPUTS)
}

=head2 get_tree

Gets the layout tree from i3 (>= v4.0).

    my $tree = i3->get_tree->recv;
    say Dumper($tree);

=cut
sub get_tree {
    my ($self) = @_;

    $self->_ensure_connection;

    $self->message(TYPE_GET_TREE)
}

=head2 get_marks

Gets all the window identifier marks from i3 (>= v4.1).

    my $marks = i3->get_marks->recv;
    say Dumper($marks);

=cut
sub get_marks {
    my ($self) = @_;

    $self->_ensure_connection;

    $self->message(TYPE_GET_MARKS)
}

=head2 get_bar_config

Gets the bar configuration for the specific bar id from i3 (>= v4.1).

    my $config = i3->get_bar_config($id)->recv;
    say Dumper($config);

=cut
sub get_bar_config {
    my ($self, $id) = @_;

    $self->_ensure_connection;

    $self->message(TYPE_GET_BAR_CONFIG, $id)
}

=head2 get_version

Gets the i3 version via IPC, with a fall-back that parses the output of i3
--version (for i3 < v4.3).

    my $version = i3->get_version()->recv;
    say "major: " . $version->{major} . ", minor = " . $version->{minor};

=cut
sub get_version {
    my ($self) = @_;

    $self->_ensure_connection;

    my $cv = AnyEvent->condvar;

    my $version_cv = $self->message(TYPE_GET_VERSION);
    my $timeout;
    $timeout = AnyEvent->timer(
        after => 1,
        cb => sub {
            warn "Falling back to i3 --version since the running i3 doesn’t support GET_VERSION yet.";
            my $version = _call_i3('--version');
            $version =~ s/^i3 version //;
            my $patch = 0;
            my ($major, $minor) = ($version =~ /^([0-9]+)\.([0-9]+)/);
            if ($version =~ /^[0-9]+\.[0-9]+\.([0-9]+)/) {
                $patch = $1;
            }
            # Strip everything from the © sign on.
            $version =~ s/ ©.*$//g;
            $cv->send({
                major => int($major),
                minor => int($minor),
                patch => int($patch),
                human_readable => $version,
            });
            undef $timeout;
        },
    );
    $version_cv->cb(sub {
        undef $timeout;
        $cv->send($version_cv->recv);
    });

    return $cv;
}

=head2 get_config

Gets the raw last read config from i3. Requires i3 >= 4.14

=cut
sub get_config {
    my ($self) = @_;

    $self->_ensure_connection;

    $self->message(TYPE_GET_CONFIG);
}


=head2 command($content)

Makes i3 execute the given command

    my $reply = i3->command("reload")->recv;
    die "command failed" unless $reply->{success};

=cut
sub command {
    my ($self, $content) = @_;

    $self->_ensure_connection;

    $self->message(TYPE_RUN_COMMAND, $content)
}

=head1 AUTHOR

Michael Stapelberg, C<< <michael at i3wm.org> >>

=head1 BUGS

Please report any bugs or feature requests to C<bug-anyevent-i3 at
rt.cpan.org>, or through the web interface at
L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=AnyEvent-I3>.  I will be
notified, and then you'll automatically be notified of progress on your bug as
I make changes.

=head1 SUPPORT

You can find documentation for this module with the perldoc command.

    perldoc AnyEvent::I3

You can also look for information at:

=over 2

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=AnyEvent-I3>

=item * The i3 window manager website

L<http://i3wm.org>

=back


=head1 ACKNOWLEDGEMENTS


=head1 LICENSE AND COPYRIGHT

Copyright 2010-2012 Michael Stapelberg.

This program is free software; you can redistribute it and/or modify it
under the terms of either: the GNU General Public License as published
by the Free Software Foundation; or the Artistic License.

See http://dev.perl.org/licenses/ for more information.


=cut

1; # End of AnyEvent::I3
