#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
# © 2010 Michael Stapelberg and contributors
package complete_run;
use strict;
use warnings;
use v5.10;
use utf8;
# the following are modules which ship with Perl (>= 5.10):
use Pod::Usage;
use File::Temp qw(tempfile tempdir);
use Getopt::Long;
use POSIX ();
use TAP::Harness;
use TAP::Parser;
use TAP::Parser::Aggregator;
use Time::HiRes qw(time);
use IO::Handle;

my $dirname;

BEGIN {
    use File::Basename;
    use Cwd qw(abs_path);

    # fileparse()[1] contains the directory portion of the specified path.
    # See File::Basename(3p) for more details.
    $dirname = (fileparse(abs_path($0)))[1];
}

# these are shipped with the testsuite
use lib $dirname . 'lib';
use i3test::Util qw(slurp);
use StartXServer;
use StatusLine;
use TestWorker;
# the following modules are not shipped with Perl
use AnyEvent;
use AnyEvent::Util;
use AnyEvent::Handle;
use AnyEvent::I3 qw(:all);
use X11::XCB::Connection;
use JSON::XS; # AnyEvent::I3 depends on it, too.

binmode STDOUT, ':utf8';
binmode STDERR, ':utf8';

# Close superfluous file descriptors which were passed by running in a VIM
# subshell or situations like that.
AnyEvent::Util::close_all_fds_except(0, 1, 2);

# convenience wrapper to write to the log file
my $log;
sub Log { say $log "@_" }

my %timings;
my $help = 0;
# Number of tests to run in parallel. Important to know how many Xephyr
# instances we need to start (unless @displays are given). Defaults to
# num_cores * 2.
my $parallel = undef;
my @displays = ();
my %options = (
    valgrind => 0,
    strace => 0,
    xtrace => 0,
    coverage => 0,
    restart => 0,
);
my $keep_xserver_output = 0;

my $result = GetOptions(
    "coverage-testing" => \$options{coverage},
    "keep-xserver-output" => \$keep_xserver_output,
    "valgrind" => \$options{valgrind},
    "strace" => \$options{strace},
    "xtrace" => \$options{xtrace},
    "display=s" => \@displays,
    "parallel=i" => \$parallel,
    "help|?" => \$help,
);

pod2usage(-verbose => 2, -exitcode => 0) if $help;

chdir $dirname or die "Could not chdir into $dirname";

# Check for missing executables
my @binaries = qw(
                   ../i3
                   ../i3bar/i3bar
                   ../i3-config-wizard/i3-config-wizard
                   ../i3-dump-log/i3-dump-log
                   ../i3-input/i3-input
                   ../i3-msg/i3-msg
                   ../i3-nagbar/i3-nagbar
               );

foreach my $binary (@binaries) {
    die "$binary executable not found, did you run “make”?" unless -e $binary;
    die "$binary is not an executable" unless -x $binary;
}

if ($options{coverage}) {
    qx(command -v lcov &> /dev/null);
    die "Cannot find lcov needed for coverage testing." if $?;
    qx(command -v genhtml &> /dev/null);
    die "Cannot find genhtml needed for coverage testing." if $?;

    # clean out the counters that may be left over from previous tests.
    qx(lcov -d ../ --zerocounters &> /dev/null);
}

qx(Xephyr -help 2>&1);
die "Xephyr was not found in your path. Please install Xephyr (xserver-xephyr on Debian)." if $?;

@displays = split(/,/, join(',', @displays));
@displays = map { s/ //g; $_ } @displays;

# 2: get a list of all testcases
my @testfiles = @ARGV;

# if no files were passed on command line, run all tests from t/
@testfiles = <t/*.t> if @testfiles == 0;

my $numtests = scalar @testfiles;

# No displays specified, let’s start some Xephyr instances.
if (@displays == 0) {
    @displays = start_xserver($parallel, $numtests, $keep_xserver_output);
}

# 1: create an output directory for this test-run
my $outdir = "testsuite-";
$outdir .= POSIX::strftime("%Y-%m-%d-%H-%M-%S-", localtime());
$outdir .= `git describe --tags`;
chomp($outdir);
mkdir($outdir) or die "Could not create $outdir";
unlink("latest") if -l "latest";
symlink("$outdir", "latest") or die "Could not symlink latest to $outdir";


# connect to all displays for two reasons:
# 1: check if the display actually works
# 2: keep the connection open so that i3 is not the only client. this prevents
#    the X server from exiting
my @single_worker;
for my $display (@displays) {
    my $screen;
    my $x = X11::XCB::Connection->new(display => $display);
    if ($x->has_error) {
        die "Could not connect to display $display\n";
    } else {
        # start a TestWorker for each display
        push @single_worker, worker($display, $x, $outdir, \%options);
    }
}

# Read previous timing information, if available. We will be able to roughly
# predict the test duration and schedule a good order for the tests.
my $timingsjson = slurp('.last_run_timings.json') if -e '.last_run_timings.json';
%timings = %{decode_json($timingsjson)} if length($timingsjson) > 0;

# Re-order the files so that those which took the longest time in the previous
# run will be started at the beginning to not delay the whole run longer than
# necessary.
@testfiles = map  { $_->[0] }
             sort { $b->[1] <=> $a->[1] }
             map  { [$_, $timings{$_} // 999] } @testfiles;

# Run 000-load-deps.t first to bail out early when dependencies are missing.
my $loadtest = "t/000-load-deps.t";
if ((scalar grep { $_ eq $loadtest } @testfiles) > 0) {
    @testfiles = ($loadtest, grep { $_ ne $loadtest } @testfiles);
}

printf("\nRough time estimate for this run: %.2f seconds\n\n", $timings{GLOBAL})
    if exists($timings{GLOBAL});

# Forget the old timings, we don’t necessarily run the same set of tests as
# before. Otherwise we would end up with left-overs.
%timings = (GLOBAL => time());

my $logfile = "$outdir/complete-run.log";
open $log, '>', $logfile or die "Could not create '$logfile': $!";
$log->autoflush(1);
say "Writing logfile to '$logfile'...";

# 3: run all tests
my @done;
my $num = @testfiles;
my $harness = TAP::Harness->new({ });

my $aggregator = TAP::Parser::Aggregator->new();
$aggregator->start();

status_init(displays => \@displays, tests => $num);

my $single_cv = AE::cv;

# We start tests concurrently: For each display, one test gets started. Every
# test starts another test after completing.
for (@single_worker) {
    $single_cv->begin;
    take_job($_, $single_cv, \@testfiles);
}

$single_cv->recv;

$aggregator->stop();

# print empty lines to separate failed tests from statuslines
print "\n\n";

for (@done) {
    my ($test, $output) = @$_;
    say "no output for $test" unless $output;
    Log "output for $test:";
    Log $output;
    # print error messages of failed tests
    say for $output =~ /^not ok.+\n+((?:^#.+\n)+)/mg
}

# 4: print summary
$harness->summary($aggregator);

close $log;

# 5: Save the timings for better scheduling/prediction next run.
$timings{GLOBAL} = time() - $timings{GLOBAL};
open(my $fh, '>', '.last_run_timings.json');
print $fh encode_json(\%timings);
close($fh);

# 6: Print the slowest test files.
my @slowest = map  { $_->[0] }
              sort { $b->[1] <=> $a->[1] }
              map  { [$_, $timings{$_}] }
              grep { !/^GLOBAL$/ } keys %timings;
say '';
say 'The slowest tests are:';
printf("\t%s with %.2f seconds\n", $_, $timings{$_})
    for @slowest[0..($#slowest > 4 ? 4 : $#slowest)];

# When we are running precisely one test, print the output. Makes developing
# with a single testcase easier.
if ($numtests == 1) {
    say '';
    say 'Test output:';
    say slurp($logfile);
}

END { cleanup() }

if ($options{coverage}) {
    print("\nGenerating test coverage report...\n");
    qx(lcov -d ../ -b ../ --capture -o latest/i3-coverage.info);
    qx(genhtml -o latest/i3-coverage latest/i3-coverage.info);
    if ($?) {
        print("Could not generate test coverage html. Did you compile i3 with test coverage support?\n");
    } else {
        print("Test coverage report generated in latest/i3-coverage\n");
    }
}

# Report logfiles that match “(Leak|Address)Sanitizer:”.
my @logs_with_leaks;
for my $log (<$outdir/i3-log-for-*>) {
    if (slurp($log) =~ /(Leak|Address)Sanitizer:/) {
        push @logs_with_leaks, $log;
    }
}
if (scalar @logs_with_leaks > 0) {
    say "\nThe following test logfiles contain AddressSanitizer or LeakSanitizer reports:";
    for my $log (sort @logs_with_leaks) {
        say "\t$log";
    }
}

exit ($aggregator->failed > 0);

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
    my ($worker, $cv, $tests) = @_;

    my $test = shift @$tests
        or return $cv->end;

    my $display = $worker->{display};

    Log status($display, "$test: starting");
    $timings{$test} = time();
    worker_next($worker, $test);

    # create a TAP::Parser with an in-memory fh
    my $output;
    my $parser = TAP::Parser->new({
        source => do { open(my $fh, '<', \$output); $fh },
    });

    my $ipc = $worker->{ipc};

    my $w;
    $w = AnyEvent->io(
        fh => $ipc,
        poll => 'r',
        cb => sub {
            state $tests_completed = 0;
            state $partial = '';

            sysread($ipc, my $buf, 4096) or die "sysread: $!";

            if ($partial) {
                $buf = $partial . $buf;
                $partial = '';
            }

            # make sure we feed TAP::Parser complete lines so it doesn't blow up
            if (substr($buf, -1, 1) ne "\n") {
                my $nl = rindex($buf, "\n");
                if ($nl == -1) {
                    $partial = $buf;
                    return;
                }

                # strip partial from buffer
                $partial = substr($buf, $nl + 1, '');
            }

            # count lines before stripping eof-marker otherwise we might
            # end up with for (1 .. 0) { } which would effectivly skip the loop
            my $lines = $buf =~ tr/\n//;
            my $t_eof = $buf =~ s/^$TestWorker::EOF$//m;

            $output .= $buf;

            for (1 .. $lines) {
                my $result = $parser->next;
                next unless defined($result);
                if ($result->is_test) {
                    $tests_completed++;
                    status($display, "$test: [$tests_completed/??] ");
                } elsif ($result->is_bailout) {
                    Log status($display, "$test: BAILOUT");
                    status_completed(scalar @done);
                    say "";
                    say "test $test bailed out: " . $result->explanation;
                    exit 1;
                }
            }

            return unless $t_eof;

            Log status($display, "$test: finished");
            $timings{$test} = time() - $timings{$test};
            status_completed(scalar @done);

            $aggregator->add($test, $parser);
            push @done, [ $test, $output ];

            undef $w;
            take_job($worker, $cv, $tests);
        }
    );
}

sub cleanup {
    my $exitcode = $?;
    $_->() for our @CLEANUP;
    exit $exitcode;
}

# must be in a begin block because we C<exit 0> above
BEGIN {
    $SIG{$_} = sub {
        require Carp; Carp::cluck("Caught SIG$_[0]\n");
        cleanup();
    } for qw(INT TERM QUIT KILL PIPE)
}

__END__

=head1 NAME

complete-run.pl - Run the i3 testsuite

=head1 SYNOPSIS

complete-run.pl [files...]

=head1 EXAMPLE

To run the whole testsuite on a reasonable number of Xephyr instances (your
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

  # Run four tests in parallel on some Xephyr servers
  ./complete-run.pl -d :1,:2,:3,:4

Note that it is not necessary to specify this anymore. If omitted,
complete-run.pl will start (num_cores * 2) Xephyr instances.

=item B<--valgrind>

Runs i3 under valgrind to find memory problems. The output will be available in
C<latest/valgrind-for-$test.log>.

=item B<--strace>

Runs i3 under strace to trace system calls. The output will be available in
C<latest/strace-for-$test.log>.

=item B<--xtrace>

Runs i3 under xtrace to trace X11 requests/replies. The output will be
available in C<latest/xtrace-for-$test.log>.

=item B<--coverage-testing>

Generates a test coverage report at C<latest/i3-coverage>. Exits i3 cleanly
during tests (instead of kill -9) to make coverage testing work properly.

=item B<--parallel>

Number of Xephyr instances to start (if you don't want to start num_cores * 2
instances for some reason).

  # Run all tests on a single Xephyr instance
  ./complete-run.pl -p 1

=back
