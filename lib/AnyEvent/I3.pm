package AnyEvent::I3;
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use JSON::XS;
use AnyEvent::Handle;
use AnyEvent::Socket;
use AnyEvent;
use Encode;

=head1 NAME

AnyEvent::I3 - communicate with the i3 window manager

=cut

our $VERSION = '0.08';

=head1 VERSION

Version 0.08

=head1 SYNOPSIS

This module connects to the i3 window manager using the UNIX socket based
IPC interface it provides (if enabled in the configuration file). You can
then subscribe to events or send messages and receive their replies.

    use AnyEvent::I3 qw(:all);

    my $i3 = i3("~/.i3/ipc.sock");

    $i3->connect->recv or die "Error connecting";
    say "Connected to i3";

    my $workspaces = $i3->message(TYPE_GET_WORKSPACES)->recv;
    say "Currently, you use " . @{$workspaces} . " workspaces";

...or, using the sugar methods:

    use AnyEvent::I3;

    my $workspaces = i3->get_workspaces->recv;
    say "Currently, you use " . @{$workspaces} . " workspaces";

=head1 EXPORT

=head2 $i3 = i3([ $path ]);

Creates a new C<AnyEvent::I3> object and returns it. C<path> is the path of
the UNIX socket to connect to.

=head1 SUBROUTINES/METHODS

=cut

use Exporter qw(import);
use base 'Exporter';

our @EXPORT = qw(i3);

use constant TYPE_COMMAND => 0;
use constant TYPE_GET_WORKSPACES => 1;
use constant TYPE_SUBSCRIBE => 2;
use constant TYPE_GET_OUTPUTS => 3;
use constant TYPE_GET_TREE => 4;
use constant TYPE_GET_MARKS => 5;

our %EXPORT_TAGS = ( 'all' => [
    qw(i3 TYPE_COMMAND TYPE_GET_WORKSPACES TYPE_SUBSCRIBE TYPE_GET_OUTPUTS TYPE_GET_TREE TYPE_GET_MARKS)
] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{all} } );

my $magic = "i3-ipc";

# TODO: auto-generate this from the header file? (i3/ipc.h)
my $event_mask = (1 << 31);
my %events = (
    workspace => ($event_mask | 0),
    output => ($event_mask | 1),
    _error => 0xFFFFFFFF,
);

sub i3 {
    AnyEvent::I3->new(@_)
}

=head2 $i3 = AnyEvent::I3->new([ $path ])

Creates a new C<AnyEvent::I3> object and returns it. C<path> is the path of
the UNIX socket to connect to.

=cut
sub new {
    my ($class, $path) = @_;

    $path ||= '~/.i3/ipc.sock';

    # Check if we need to resolve ~
    if ($path =~ /~/) {
        # We use getpwuid() instead of $ENV{HOME} because the latter is tainted
        # and thus produces warnings when running tests with perl -T
        my $home = (getpwuid($<))[7];
        die "Could not get home directory" unless $home and -d $home;
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

    if ($i3->subscribe(\%callbacks)->recv->{success})
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

    my $reply = $i3->message(TYPE_COMMAND, "reload")->recv;
    if ($reply->{success}) {
        say "Configuration successfully reloaded";
    }

=cut
sub message {
    my ($self, $type, $content) = @_;

    die "No message type specified" unless defined($type);

    die "No connection to i3" unless defined($self->{ipchdl});

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

    $self->connect->recv or die "Unable to connect to i3"
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

=head2 command($content)

Makes i3 execute the given command

    my $reply = i3->command("reload")->recv;
    die "command failed" unless $reply->{success};

=cut
sub command {
    my ($self, $content) = @_;

    $self->_ensure_connection;

    $self->message(TYPE_COMMAND, $content)
}

=head1 AUTHOR

Michael Stapelberg, C<< <michael at stapelberg.de> >>

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

L<http://i3.zekjur.net/>

=back


=head1 ACKNOWLEDGEMENTS


=head1 LICENSE AND COPYRIGHT

Copyright 2010 Michael Stapelberg.

This program is free software; you can redistribute it and/or modify it
under the terms of either: the GNU General Public License as published
by the Free Software Foundation; or the Artistic License.

See http://dev.perl.org/licenses/ for more information.


=cut

1; # End of AnyEvent::I3
