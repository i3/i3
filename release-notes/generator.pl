#!/usr/bin/env perl
use strict;
use warnings;
use v5.10;
use Getopt::Long;

my @template = (
'
 ┌──────────────────────────────┐
 │ Release notes for i3 v4.21   │
 └──────────────────────────────┘

This is i3 v4.21. This version is considered stable. All users of i3 are
strongly encouraged to upgrade.


 ┌────────────────────────────┐
 │ Changes in i3 v4.21        │
 └────────────────────────────┘

',
'
 ┌────────────────────────────┐
 │ Bugfixes                   │
 └────────────────────────────┘

');

my $print_urls = 0;
my $result = GetOptions('print-urls' => \$print_urls);

sub get_number {
  my $s = shift;
  return $1 if $s =~ m/^(\d+)/;
  return -1;
}

sub read_changefiles {
    my $dirpath = shift;
    opendir my $dir, $dirpath or die "Cannot open directory $dirpath: $!";
    my @files = sort { get_number($a) <=> get_number($b) } readdir $dir;

    closedir $dir;

    my $s = '';
    for my $filename (@files) {
        next if $filename eq '.';
        next if $filename eq '..';
        next if $filename eq '0-example';

        die "Filename $filename should start with a number (e.g. the pull request number)" unless get_number($filename) > 0;

        $filename = $dirpath . '/' . $filename;
        open my $in, '<', $filename or die "can't open $filename: $!";
        my @lines = <$in>;
        close $in or die "can't close $filename: $!";

        my $content = trim(join("\n  ", map { trim($_) } @lines));
        die "$filename can't be empty" unless length($content) > 0;

        my $url = '';
        if ($print_urls) {
            my $commit = `git log --diff-filter=A --pretty=format:"%H" $filename`;
            $commit = trim($commit) if defined($commit);
            die "$filename: git log failed to find commit" if ($?) || (length($commit) == 0);

            my $pr = find_pr($commit);
            $url = 'https://github.com/i3/i3/commit/' . $commit;
            $url = 'https://github.com/i3/i3/pull/' . $pr if defined($pr);
            $url = $url . "\n";
        }

        $s = $s . '  • ' . $content . "\n" . $url;
    }
    return $s;
}

sub find_pr {
    my $hash = shift;
    my $result = `git log --merges --ancestry-path --oneline $hash..next | grep 'Merge pull request' | tail -n1`;
    return unless defined($result);

    return unless ($result =~ /Merge pull request .([0-9]+)/);
    return $1;
}

sub trim {
    (my $s = $_[0]) =~ s/^\s+|\s+$//g;
    return $s;
}

# Expected to run for i3's git root
my $changes = read_changefiles('release-notes/changes');
my $bugfixes = read_changefiles('release-notes/bugfixes');

print $template[0] . $changes . $template[1] . $bugfixes;
