package i3test::Util;
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use v5.10;

use X11::XCB qw(GET_PROPERTY_TYPE_ANY);
use X11::XCB::Connection;

use Exporter qw(import);
our @EXPORT = qw(
    slurp
    get_socket_path
);

=encoding utf-8

=head1 NAME

i3test::Util - General utility functions

=cut

=head1 EXPORT

=cut

=head2 slurp($fn)

Reads the entire file specified in the arguments and returns the content.

=cut
sub slurp {
    my ($file) = @_;
    my $content = do {
        local $/ = undef;
        open my $fh, "<", $file or die "could not open $file: $!";
        <$fh>;
    };

    return $content;
}

=head2 get_socket_path([X11::XCB::Connection])

Gets the socket path from the C<I3_SOCKET_PATH> atom stored on the X11 root
window.

=cut
sub get_socket_path {
    my ($x) = @_;
    $x //= X11::XCB::Connection->new();
    my $atom = $x->atom(name => 'I3_SOCKET_PATH');
    my $cookie = $x->get_property(0, $x->get_root_window(), $atom->id, GET_PROPERTY_TYPE_ANY, 0, 256);
    my $reply = $x->get_property_reply($cookie->{sequence});
    my $socketpath = $reply->{value};
    if ($socketpath eq "/tmp/nested-$ENV{DISPLAY}") {
        $socketpath .= '-activation';
    }
    return $socketpath;
}

=head1 AUTHOR

Michael Stapelberg <michael@i3wm.org>

=cut

1
