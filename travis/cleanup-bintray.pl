#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use Data::Dumper;
use HTTP::Tiny; # in core since v5.13.9
use JSON::PP; # in core since v5.13.9
use MIME::Base64; # in core since v5.7
use v5.13;

my $repo = shift;

my $auth = $ENV{'BINTRAY_USER'} . ':' . $ENV{'BINTRAY_KEY'};
die "BINTRAY_USER and/or BINTRAY_KEY environment variables not set" if $auth eq ':';
# TODO(stapelberg): switch to putting $auth into the URL once perl-modules ≥
# 5.20 is available on travis (Ubuntu Wily or newer).
my $auth_header = 'Basic ' . MIME::Base64::encode_base64($auth, "");
my $apiurl = 'https://api.bintray.com/packages/i3/' . $repo . '/i3-wm';
my $client = HTTP::Tiny->new(
    verify_SSL => 1,
    default_headers => {
        'authorization' => $auth_header,
    });
my $resp = $client->get($apiurl);
die "Getting versions failed: HTTP status $resp->{status} (content: $resp->{content})" unless $resp->{success};
my $decoded = decode_json($resp->{content});
my @versions = reverse sort {
    (system("/usr/bin/dpkg", "--compare-versions", "$a", "gt", "$b") == 0) ? 1 : -1
} @{$decoded->{versions}};

# Keep the most recent 5 versions.
splice(@versions, 0, 5);

for my $version (@versions) {
    say "Deleting old version $version";
    $resp = $client->request('DELETE', "$apiurl/versions/$version");
    die "Deletion of version $version failed: HTTP status $resp->{status} (content: $resp->{content})" unless $resp->{success};
}
