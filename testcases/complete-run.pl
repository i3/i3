#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
#
# © 2010-2011 Michael Stapelberg and contributors
#
# syntax: ./complete-run.pl --display :1 --display :2
# to run the test suite on the X11 displays :1 and :2
# use 'Xdummy :1' and 'Xdummy :2' before to start two
# headless X11 servers
#

use strict;
use warnings;
use v5.10;
# the following are modules which ship with Perl (>= 5.10):
use Cwd qw(abs_path);
use File::Basename qw(basename);
use File::Temp qw(tempfile tempdir);
use Getopt::Long;
use IO::Socket::UNIX;
use POSIX;
use Time::HiRes qw(sleep gettimeofday tv_interval);
use TAP::Harness;
use TAP::Parser;
use TAP::Parser::Aggregator;
# these are shipped with the testsuite
use lib qw(lib);
use SocketActivation;
# the following modules are not shipped with Perl
use EV;
use AnyEvent;
use AnyEvent::Handle;
use AnyEvent::I3 qw(:all);
use X11::XCB;

# install a dummy CHLD handler to overwrite the CHLD handler of AnyEvent / EV
# XXX: we could maybe also use a different loop than the default loop in EV?
$SIG{CHLD} = sub {
};

# reads in a whole file
sub slurp {
    open my $fh, '<', shift;
    local $/;
    <$fh>;
}

my $coverage_testing = 0;
my @displays = ();

my $result = GetOptions(
    "coverage-testing" => \$coverage_testing,
    "display=s" => \@displays,
);

@displays = split(/,/, join(',', @displays));
@displays = map { s/ //g; $_ } @displays;

@displays = qw(:1) if @displays == 0;

# connect to all displays for two reasons:
# 1: check if the display actually works
# 2: keep the connection open so that i3 is not the only client. this prevents
#    the X server from exiting (Xdummy will restart it, but not quick enough
#    sometimes)
my @conns;
my @wdisplays;
for my $display (@displays) {
    my $screen;
    my $x = X11::XCB->new($display, $screen);
    if ($x->has_error) {
        say STDERR "WARNING: Not using X11 display $display, could not connect";
    } else {
        push @conns, $x;
        push @wdisplays, $display;
    }
}

my $config = slurp('i3-test.config');

# 1: get a list of all testcases
my @testfiles = @ARGV;

# if no files were passed on command line, run all tests from t/
@testfiles = <t/*.t> if @testfiles == 0;

# 2: create an output directory for this test-run
my $outdir = "testsuite-";
$outdir .= POSIX::strftime("%Y-%m-%d-%H-%M-%S-", localtime());
$outdir .= `git describe --tags`;
chomp($outdir);
mkdir($outdir) or die "Could not create $outdir";
unlink("latest") if -e "latest";
symlink("$outdir", "latest") or die "Could not symlink latest to $outdir";

# 3: run all tests
my @done;
my $num = @testfiles;
my $harness = TAP::Harness->new({ });

my $aggregator = TAP::Parser::Aggregator->new();
$aggregator->start();

my $cv = AnyEvent->condvar;

# We start tests concurrently: For each display, one test gets started. Every
# test starts another test after completing.
take_job($_) for @wdisplays;

#
# Takes a test from the beginning of @testfiles and runs it.
#
# The TAP::Parser (which reads the test output) will get called as soon as
# there is some activity on the stdout file descriptor of the test process
# (using an AnyEvent->io watcher).
#
# When a test completes and @done contains $num entries, the $cv condvar gets
# triggered to finish testing.
#
sub take_job {
    my ($display) = @_;

    my $test = shift @testfiles;
    return unless $test;
    my $dont_start = (slurp($test) =~ /# !NO_I3_INSTANCE!/);
    my $logpath = "$outdir/i3-log-for-" . basename($test);

    my ($fh, $tmpfile) = tempfile('i3-run-cfg.XXXXXX', UNLINK => 1);
    say $fh $config;
    say $fh "ipc-socket /tmp/nested-$display";
    close($fh);

    my $activate_cv = AnyEvent->condvar;
    my $time_before_start = [gettimeofday];

    my $pid;
    if (!$dont_start) {
        $pid = activate_i3(
            unix_socket_path => "/tmp/nested-$display-activation",
            display => $display,
            configfile => $tmpfile,
            logpath => $logpath,
            cv => $activate_cv
        );

        my $child_watcher;
        $child_watcher = AnyEvent->child(pid => $pid, cb => sub {
            say "child died. pid = $pid";
            undef $child_watcher;
        });
    }

    my $kill_i3 = sub {
        # Don’t bother killing i3 when we haven’t started it
        return if $dont_start;

        # When measuring code coverage, try to exit i3 cleanly (otherwise, .gcda
        # files are not written) and fallback to killing it
        if ($coverage_testing) {
            my $exited = 0;
            eval {
                say "Exiting i3 cleanly...";
                i3("/tmp/nested-$display")->command('exit')->recv;
                $exited = 1;
            };
            return if $exited;
        }

        say "[$display] killing i3";
        kill(9, $pid) or die "could not kill i3";
    };

    # This will be called as soon as i3 is running and answered to our
    # IPC request
    $activate_cv->cb(sub {
        my $time_activating = [gettimeofday];
        my $start_duration = tv_interval($time_before_start, $time_activating);
        my ($status) = $activate_cv->recv;
        if ($dont_start) {
            say "[$display] Not starting i3, testcase does that";
        } else {
            say "[$display] i3 startup: took " . sprintf("%.2f", $start_duration) . "s, status = $status";
        }

        say "[$display] Running $test with logfile $logpath";

        my $output;
        open(my $spool, '>', \$output);
        my $parser = TAP::Parser->new({
            exec => [ 'sh', '-c', qq|DISPLAY=$display LOGPATH="$logpath" /usr/bin/perl -Ilib $test| ],
            spool => $spool,
            merge => 1,
        });

        my @watchers;
        my ($stdout, $stderr) = $parser->get_select_handles;
        for my $handle ($parser->get_select_handles) {
            my $w;
            $w = AnyEvent->io(
                fh => $handle,
                poll => 'r',
                cb => sub {
                    # Ignore activity on stderr (unnecessary with merge => 1,
                    # but let’s keep it in here if we want to use merge => 0
                    # for some reason in the future).
                    return if defined($stderr) and $handle == $stderr;

                    my $result = $parser->next;
                    if (defined($result)) {
                        # TODO: check if we should bail out
                        return;
                    }

                    # $result is not defined, we are done parsing
                    say "[$display] $test finished";
                    close($parser->delete_spool);
                    $aggregator->add($test, $parser);
                    push @done, [ $test, $output ];

                    $kill_i3->();

                    undef $_ for @watchers;
                    if (@done == $num) {
                        $cv->send;
                    } else {
                        take_job($display);
                    }
                }
            );
            push @watchers, $w;
        }
    });

    $activate_cv->send(1) if $dont_start;
}

$cv->recv;

$aggregator->stop();

for (@done) {
    my ($test, $output) = @$_;
    say "output for $test:";
    say $output;
}

# 4: print summary
$harness->summary($aggregator);
