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
use lib '/usr/share/lintian/lib';
use i3test::Util qw(slurp);
use Lintian::Spelling qw(check_spelling);

# Lintian complains if we don’t set a vendor.
use Lintian::Data;
use Lintian::Profile;

my $profile = Lintian::Profile->new;
$profile->load('debian', ['/usr/share/lintian']);

my $exitcode = 0;

# Whitelist for spelling errors in manpages, in case the spell checker has
# false-positives.
my $binary_spelling_exceptions = [
    #'exmaple', # Example for how to add entries to this whitelist.
    'betwen', # asan_flags.inc contains this spelling error.
    'dissassemble', # https://reviews.llvm.org/D93902
    'oT', # lintian finds this in build/i3bar when built with clang?!
];
my @binaries = qw(
    build/i3
    build/i3-config-wizard
    build/i3-dump-log
    build/i3-input
    build/i3-msg
    build/i3-nagbar
    build/i3bar
);
for my $binary (@binaries) {
    check_spelling($profile->data, slurp($binary), $binary_spelling_exceptions, sub {
        my ($current, $fixed) = @_;
        say STDERR qq|Binary "$binary" contains a spelling error: "$current" should be "$fixed"|;
        $exitcode = 1;
    });
}

# Whitelist for spelling errors in manpages, in case the spell checker has
# false-positives.
my $manpage_spelling_exceptions = [
];

for my $name (glob('build/man/*.1')) {
    for my $line (split(/\n/, slurp($name))) {
        next if $line =~ /^\.\\\"/o;
        check_spelling($profile->data, $line, $manpage_spelling_exceptions, sub {
            my ($current, $fixed) = @_;
            say STDERR qq|Manpage "$name" contains a spelling error: "$current" should be "$fixed"|;
            $exitcode = 1;
        });
    }
}

exit $exitcode;
