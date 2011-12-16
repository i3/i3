#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
# © 2010-2011 Michael Stapelberg and contributors
package complete_run;
use strict;
use warnings;
use v5.10;
# the following are modules which ship with Perl (>= 5.10):
use Pod::Usage;
use Cwd qw(abs_path);
use File::Temp qw(tempfile tempdir);
use Getopt::Long;
use POSIX ();
use TAP::Harness;
use TAP::Parser;
use TAP::Parser::Aggregator;
use Time::HiRes qw(time);
# these are shipped with the testsuite
use lib qw(lib);
use StartXDummy;
use StatusLine;
use TestWorker;
# the following modules are not shipped with Perl
use AnyEvent;
use AnyEvent::Util;
use AnyEvent::Handle;
use AnyEvent::I3 qw(:all);
use X11::XCB::Connection;
use JSON::XS; # AnyEvent::I3 depends on it, too.

# Close superfluous file descriptors which were passed by running in a VIM
# subshell or situations like that.
AnyEvent::Util::close_all_fds_except(0, 1, 2);

# convinience wrapper to write to the log file
my $log;
sub Log { say $log "@_" }

my %timings;
my $coverage_testing = 0;
my $valgrind = 0;
my $strace = 0;
my $help = 0;
# Number of tests to run in parallel. Important to know how many Xdummy
# instances we need to start (unless @displays are given). Defaults to
# num_cores * 2.
my $parallel = undef;
my @displays = ();

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
@displays = start_xdummy($parallel) if @displays == 0;

# 1: create an output directory for this test-run
my $outdir = "testsuite-";
$outdir .= POSIX::strftime("%Y-%m-%d-%H-%M-%S-", localtime());
$outdir .= `git describe --tags`;
chomp($outdir);
mkdir($outdir) or die "Could not create $outdir";
unlink("latest") if -e "latest";
symlink("$outdir", "latest") or die "Could not symlink latest to $outdir";


# connect to all displays for two reasons:
# 1: check if the display actually works
# 2: keep the connection open so that i3 is not the only client. this prevents
#    the X server from exiting (Xdummy will restart it, but not quick enough
#    sometimes)
my @worker;
for my $display (@displays) {
    my $screen;
    my $x = X11::XCB::Connection->new(display => $display);
    if ($x->has_error) {
        die "Could not connect to display $display\n";
    } else {
        # start a TestWorker for each display
        push @worker, worker($display, $x, $outdir);
    }
}

# Read previous timing information, if available. We will be able to roughly
# predict the test duration and schedule a good order for the tests.
my $timingsjson = StartXDummy::slurp('.last_run_timings.json');
%timings = %{decode_json($timingsjson)} if length($timingsjson) > 0;

# 2: get a list of all testcases
my @testfiles = @ARGV;

# if no files were passed on command line, run all tests from t/
@testfiles = <t/*.t> if @testfiles == 0;

# Re-order the files so that those which took the longest time in the previous
# run will be started at the beginning to not delay the whole run longer than
# necessary.
@testfiles = map  { $_->[0] }
             sort { $b->[1] <=> $a->[1] }
             map  { [$_, $timings{$_} // 999] } @testfiles;

printf("\nRough time estimate for this run: %.2f seconds\n\n", $timings{GLOBAL})
    if exists($timings{GLOBAL});

# Forget the old timings, we don’t necessarily run the same set of tests as
# before. Otherwise we would end up with left-overs.
%timings = (GLOBAL => time());

my $logfile = "$outdir/complete-run.log";
open $log, '>', $logfile or die "Could not create '$logfile': $!";
say "Writing logfile to '$logfile'...";

# 3: run all tests
my @done;
my $num = @testfiles;
my $harness = TAP::Harness->new({ });

my $aggregator = TAP::Parser::Aggregator->new();
$aggregator->start();

status_init(displays => \@displays, tests => $num);

my $cv = AE::cv;

# We start tests concurrently: For each display, one test gets started. Every
# test starts another test after completing.
for (@worker) { $cv->begin; take_job($_) }

$cv->recv;

$aggregator->stop();

# print empty lines to seperate failed tests from statuslines
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
printf("\t%s with %.2f seconds\n", $_, $timings{$_}) for @slowest[0..4];

END { cleanup() }

exit 0;

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
    my ($worker) = @_;

    my $test = shift @testfiles
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
                if (defined($result) and $result->is_test) {
                    $tests_completed++;
                    status($display, "$test: [$tests_completed/??] ");
                }
            }

            return unless $t_eof;

            Log status($display, "$test: finished");
            $timings{$test} = time() - $timings{$test};
            status_completed(scalar @done);

            $aggregator->add($test, $parser);
            push @done, [ $test, $output ];

            undef $w;
            take_job($worker);
        }
    );
}

sub cleanup {
    $_->() for our @CLEANUP;
    exit;
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
