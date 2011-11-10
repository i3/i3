#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
# © 2010-2011 Michael Stapelberg and contributors

use strict;
use warnings;
use v5.10;
# the following are modules which ship with Perl (>= 5.10):
use Pod::Usage;
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
use StartXDummy;
use StatusLine;
# the following modules are not shipped with Perl
use AnyEvent;
use AnyEvent::Util;
use AnyEvent::Handle;
use AnyEvent::I3 qw(:all);
use X11::XCB;

# Close superfluous file descriptors which were passed by running in a VIM
# subshell or situations like that.
AnyEvent::Util::close_all_fds_except(0, 1, 2);

# We actually use AnyEvent to make sure it loads an event loop implementation.
# Afterwards, we overwrite SIGCHLD:
my $cv = AnyEvent->condvar;

# Install a dummy CHLD handler to overwrite the CHLD handler of AnyEvent.
# AnyEvent’s handler wait()s for every child which conflicts with TAP (TAP
# needs to get the exit status to determine if a test is successful).
$SIG{CHLD} = sub {
};

# reads in a whole file
sub slurp {
    open(my $fh, '<', shift);
    local $/;
    <$fh>;
}

# convinience wrapper to write to the log file
my $log;
sub Log { say $log "@_" }

my $coverage_testing = 0;
my $valgrind = 0;
my $strace = 0;
my $help = 0;
# Number of tests to run in parallel. Important to know how many Xdummy
# instances we need to start (unless @displays are given). Defaults to
# num_cores * 2.
my $parallel = undef;
my @displays = ();
my @childpids = ();

my $result = GetOptions(
    "coverage-testing" => \$coverage_testing,
    "valgrind" => \$valgrind,
    "strace" => \$strace,
    "display=s" => \@displays,
    "parallel=i" => \$parallel,
    "help|?" => \$help,
);

pod2usage(-verbose => 2, -exitcode => 0) if $help;

@displays = split(/,/, join(',', @displays));
@displays = map { s/ //g; $_ } @displays;

# No displays specified, let’s start some Xdummy instances.
if (@displays == 0) {
    my ($displays, $pids) = start_xdummy($parallel);
    @displays = @$displays;
    @childpids = @$pids;
}

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
        Log "WARNING: Not using X11 display $display, could not connect";
    } else {
        push @conns, $x;
        push @wdisplays, $display;
    }
}

die "No usable displays found" if @wdisplays == 0;

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

my $logfile = "$outdir/complete-run.log";
open $log, '>', $logfile or die "Could not create '$logfile': $!";
say "Writing logfile to '$logfile'...";

# 3: run all tests
my @done;
my $num = @testfiles;
my $harness = TAP::Harness->new({ });

my $aggregator = TAP::Parser::Aggregator->new();
$aggregator->start();

status_init(displays => \@wdisplays, tests => $num);

# We start tests concurrently: For each display, one test gets started. Every
# test starts another test after completing.
for (@wdisplays) { $cv->begin; take_job($_) }

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

    my $test = shift @testfiles
        or return $cv->end;

    my $dont_start = (slurp($test) =~ /# !NO_I3_INSTANCE!/);
    my $basename = basename($test);
    my $logpath = "$outdir/i3-log-for-$basename";

    my ($fh, $tmpfile) = tempfile("i3-cfg-for-$basename.XXXXXX", UNLINK => 1);
    say $fh $config;
    say $fh "ipc-socket /tmp/nested-$display";
    close($fh);

    my $activate_cv = AnyEvent->condvar;
    my $time_before_start = [gettimeofday];

    my $pid;
    if ($dont_start) {
        $activate_cv->send(1);
    } else {
        $pid = activate_i3(
            unix_socket_path => "/tmp/nested-$display-activation",
            display => $display,
            configfile => $tmpfile,
            outdir => $outdir,
            testname => $basename,
            valgrind => $valgrind,
            strace => $strace,
            cv => $activate_cv
        );

        my $child_watcher;
        $child_watcher = AnyEvent->child(pid => $pid, cb => sub {
            Log status($display, "child died. pid = $pid");
            undef $child_watcher;
        });
    }

    my $kill_i3 = sub {
        my $kill_cv = AnyEvent->condvar;

        # Don’t bother killing i3 when we haven’t started it
        if ($dont_start) {
            $kill_cv->send();
            return $kill_cv;
        }

        # When measuring code coverage, try to exit i3 cleanly (otherwise, .gcda
        # files are not written) and fallback to killing it
        if ($coverage_testing || $valgrind) {
            my $exited = 0;
            Log status($display, 'Exiting i3 cleanly...');
            my $i3 = i3("/tmp/nested-$display");
            $i3->connect->cb(sub {
                if (!$_[0]->recv) {
                    # Could not connect to i3, just kill -9 it
                    kill(9, $pid) or die "Could not kill i3 using kill($pid)";
                    $kill_cv->send();
                } else {
                    # Connected. Now send exit and continue once that’s acked.
                    $i3->command('exit')->cb(sub {
                        $kill_cv->send();
                    });
                }
            });
        } else {
            Log status($display, 'killing i3');

            # No coverage testing or valgrind? Just kill -9 i3.
            kill(9, $pid) or die "Could not kill i3 using kill($pid)";
            $kill_cv->send();
        }

        return $kill_cv;
    };

    # This will be called as soon as i3 is running and answered to our
    # IPC request
    $activate_cv->cb(sub {
        my $time_activating = [gettimeofday];
        my $start_duration = tv_interval($time_before_start, $time_activating);
        my ($status) = $activate_cv->recv;
        if ($dont_start) {
            Log status($display, 'Not starting i3, testcase does that');
        } else {
            my $duration = sprintf("%.2f", $start_duration);
            Log status($display, "i3 startup: took $duration sec, status = $status");
        }

        Log status($display, "Starting $test");

        my $output;
        open(my $spool, '>', \$output);
        my $parser = TAP::Parser->new({
            exec => [ 'sh', '-c', qq|DISPLAY=$display TESTNAME="$basename" OUTDIR="$outdir" VALGRIND=$valgrind STRACE=$strace /usr/bin/perl -Ilib $test| ],
            spool => $spool,
            merge => 1,
        });

        my $tests_completed;

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
                        $tests_completed++;
                        status($display, "Running $test: [$tests_completed/??]");
                        # TODO: check if we should bail out
                        return;
                    }

                    # $result is not defined, we are done parsing
                    Log status($display, "$test finished");
                    close($parser->delete_spool);
                    $aggregator->add($test, $parser);
                    push @done, [ $test, $output ];

                    status_completed(scalar @done);

                    my $exitcv = $kill_i3->();
                    $exitcv->cb(sub {

                        undef $_ for @watchers;
                        if (@done == $num) {
                            $cv->end;
                        } else {
                            take_job($display);
                        }
                    });
                }
            );
            push @watchers, $w;
        }
    });
}

$cv->recv;

$aggregator->stop();

# print empty lines to seperate failed tests from statuslines
print "\n\n";

for (@done) {
    my ($test, $output) = @$_;
    Log "output for $test:";
    Log $output;
    # print error messages of failed tests
    say for $output =~ /^not ok.+\n+((?:^#.+\n)+)/mg
}

# 4: print summary
$harness->summary($aggregator);

close $log;

kill(15, $_) for @childpids;

__END__

=head1 NAME

complete-run.pl - Run the i3 testsuite

=head1 SYNOPSIS

complete-run.pl [files...]

=head1 EXAMPLE

To run the whole testsuite on a reasonable number of Xdummy instances (your
running X11 will not be touched), run:
  ./complete-run.pl

To run only a specific test (useful when developing a new feature), run:
  ./complete-run t/100-fullscreen.t

=head1 OPTIONS

=over 8

=item B<--display>

Specifies which X11 display should be used. Can be specified multiple times and
will parallelize the tests:

  # Run tests on the second X server
  ./complete-run.pl -d :1

  # Run four tests in parallel on some Xdummy servers
  ./complete-run.pl -d :1,:2,:3,:4

Note that it is not necessary to specify this anymore. If omitted,
complete-run.pl will start (num_cores * 2) Xdummy instances.

=item B<--valgrind>

Runs i3 under valgrind to find memory problems. The output will be available in
C<latest/valgrind-for-$test.log>.

=item B<--strace>

Runs i3 under strace to trace system calls. The output will be available in
C<latest/strace-for-$test.log>.

=item B<--coverage-testing>

Exits i3 cleanly (instead of kill -9) to make coverage testing work properly.

=item B<--parallel>

Number of Xdummy instances to start (if you don’t want to start num_cores * 2
instances for some reason).

  # Run all tests on a single Xdummy instance
  ./complete-run.pl -p 1
