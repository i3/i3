package i3test::Util;
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use v5.10;

use Exporter qw(import);
our @EXPORT = qw(
    slurp
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

=head1 AUTHOR

Michael Stapelberg <michael@i3wm.org>

=cut

1
