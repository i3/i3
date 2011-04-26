#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use v5.10;
use DateTime;
use Data::Dumper;
use Cwd qw(abs_path);
use Proc::Background;
use TAP::Harness;
use TAP::Parser::Aggregator;
use File::Basename qw(basename);

# reads in a whole file
sub slurp {
    open my $fh, '<', shift;
    local $/;
    <$fh>;
}

my $i3cmd = "export DISPLAY=:0; exec " . abs_path("../i3") . " -V -d all --disable-signalhandler -c " . abs_path("../i3.config");

# 1: get a list of all testcases
my @testfiles = @ARGV;

# if no files were passed on command line, run all tests from t/
@testfiles = <t/*.t> if @testfiles == 0;

# 2: create an output directory for this test-run
my $outdir = "testsuite-";
$outdir .= DateTime->now->strftime("%Y-%m-%d-%H-%M-%S-");
$outdir .= `git describe --tags`;
chomp($outdir);
mkdir($outdir) or die "Could not create $outdir";
unlink("latest") if -e "latest";
symlink("$outdir", "latest") or die "Could not symlink latest to $outdir";

# 3: run all tests
my $harness = TAP::Harness->new({
    verbosity => 1,
    lib => [ 't/lib' ]
});
my $aggregator = TAP::Parser::Aggregator->new();
$aggregator->start();
for my $t (@testfiles) {
    my $logpath = "$outdir/i3-log-for-" . basename($t);
    my $cmd = "$i3cmd >$logpath 2>&1";
    my $dont_start = (slurp($t) =~ /# !NO_I3_INSTANCE!/);

    my $process = Proc::Background->new($cmd) unless $dont_start;
    say "testing $t with logfile $logpath";
    $harness->aggregate_tests($aggregator, [ $t ]);
    kill(9, $process->pid) or die "could not kill i3" unless $dont_start;
}
$aggregator->stop();

# 4: print summary
$harness->summary($aggregator);
