#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
#
# © 2016 Michael Stapelberg
#
# Checks for spelling errors in binaries and manpages (to be run by continuous
# integration to point out spelling errors before accepting contributions).

use strict;
use warnings;
use v5.10;
use autodie;
use lib 'testcases/lib';
use i3test::Util qw(slurp);
use Lintian::Check qw(check_spelling);

# Lintian complains if we don’t set a vendor.
use Lintian::Data;
use Lintian::Profile;
Lintian::Data->set_vendor(
    Lintian::Profile->new('debian', ['/usr/share/lintian'], {}));

my $exitcode = 0;

# Whitelist for spelling errors in manpages, in case the spell checker has
# false-positives.
my $binary_spelling_exceptions = {
    #'exmaple' => 1, # Example for how to add entries to this whitelist.
    'betwen' => 1, # asan_flags.inc contains this spelling error.
};
my @binaries = qw(
    build/i3
    build/i3-config-wizard/i3-config-wizard
    build/i3-dump-log/i3-dump-log
    build/i3-input/i3-input
    build/i3-msg/i3-msg
    build/i3-nagbar/i3-nagbar
    build/i3bar/i3bar
);
for my $binary (@binaries) {
    check_spelling(slurp($binary), $binary_spelling_exceptions, sub {
        my ($current, $fixed) = @_;
        say STDERR qq|Binary "$binary" contains a spelling error: "$current" should be "$fixed"|;
        $exitcode = 1;
    });
}

# Whitelist for spelling errors in manpages, in case the spell checker has
# false-positives.
my $manpage_spelling_exceptions = {
};

for my $name (glob('build/man/*.1')) {
    for my $line (split(/\n/, slurp($name))) {
        next if $line =~ /^\.\\\"/o;
        check_spelling($line, $manpage_spelling_exceptions, sub {
            my ($current, $fixed) = @_;
            say STDERR qq|Manpage "$name" contains a spelling error: "$current" should be "$fixed"|;
            $exitcode = 1;
        });
    }
}

exit $exitcode;
