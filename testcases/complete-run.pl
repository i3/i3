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
use EV;
use AnyEvent;
use IO::Scalar; # not in core :\
use File::Temp qw(tempfile tempdir);
use v5.10;
use DateTime;
use Data::Dumper;
use Cwd qw(abs_path);
use Proc::Background;
use TAP::Harness;
use TAP::Parser;
use TAP::Parser::Aggregator;
use File::Basename qw(basename);
use AnyEvent::I3 qw(:all);
use Try::Tiny;
use Getopt::Long;
use Time::HiRes qw(sleep);
use X11::XCB::Connection;
use IO::Socket::UNIX; # core
use POSIX; # core
use AnyEvent::Handle;

# open a file so that we get file descriptor 3. we will later close it in the
# child and dup() the listening socket file descriptor to 3 to pass it to i3
open(my $reserved, '<', '/dev/null');
if (fileno($reserved) != 3) {
    warn "Socket file descriptor is not 3.";
    warn "Please don't start this script within a subshell of vim or something.";
    exit 1;
}

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
    try {
        my $x = X11::XCB::Connection->new(display => $display);
        push @conns, $x;
        push @wdisplays, $display;
    } catch {
        say STDERR "WARNING: Not using X11 display $display, could not connect";
    };
}

my $config = slurp('i3-test.config');

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

    my ($fh, $tmpfile) = tempfile();
    say $fh $config;
    say $fh "ipc-socket /tmp/nested-$display";
    close($fh);

    my $activate_cv = AnyEvent->condvar;
    my $start_i3 = sub {
        # remove the old unix socket
        unlink("/tmp/nested-$display-activation");

        # pass all file descriptors up to three to the children.
        # we need to set this flag before opening the socket.
        open(my $fdtest, '<', '/dev/null');
        $^F = fileno($fdtest);
        close($fdtest);
        my $socket = IO::Socket::UNIX->new(
            Listen => 1,
            Local => "/tmp/nested-$display-activation",
        );

        my $pid = fork;
        if (!defined($pid)) {
            die "could not fork()";
        }
        say "pid = $pid";
        if ($pid == 0) {
            say "child!";
            $ENV{LISTEN_PID} = $$;
            $ENV{LISTEN_FDS} = 1;
            $ENV{DISPLAY} = $display;
            $^F = 3;

            say "fileno is " . fileno($socket);
            close($reserved);
            POSIX::dup2(fileno($socket), 3);

            # now execute i3
            my $i3cmd = abs_path("../i3") . " -V -d all --disable-signalhandler";
            my $cmd = "exec $i3cmd -c $tmpfile >$logpath 2>&1";
            exec "/bin/sh", '-c', $cmd;

            # if we are still here, i3 could not be found or exec failed. bail out.
            exit 1;
        }

        my $child_watcher;
        $child_watcher = AnyEvent->child(pid => $pid, cb => sub {
            say "child died. pid = $pid";
            undef $child_watcher;
        });

        # close the socket, the child process should be the only one which keeps a file
        # descriptor on the listening socket.
        $socket->close;

        # We now connect (will succeed immediately) and send a request afterwards.
        # As soon as the reply is there, i3 is considered ready.
        my $cl = IO::Socket::UNIX->new(Peer => "/tmp/nested-$display-activation");
        my $hdl;
        $hdl = AnyEvent::Handle->new(fh => $cl, on_error => sub { $activate_cv->send(0) });

        # send a get_tree message without payload
        $hdl->push_write('i3-ipc' . pack("LL", 0, 4));

        # wait for the reply
        $hdl->push_read(chunk => 1, => sub {
            my ($h, $line) = @_;
            say "read something from i3";
            $activate_cv->send(1);
            undef $hdl;
        });

        return $pid;
    };

    my $pid;
    $pid = $start_i3->() unless $dont_start;

    my $kill_i3 = sub {
        # Don’t bother killing i3 when we haven’t started it
        return if $dont_start;

        # When measuring code coverage, try to exit i3 cleanly (otherwise, .gcda
        # files are not written) and fallback to killing it
        if ($coverage_testing) {
            my $exited = 0;
            try {
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
        say "cb";
        my ($status) = $activate_cv->recv;
        say "complete-run: status = $status";

        say "[$display] Running $test with logfile $logpath";

        my $output;
        my $parser = TAP::Parser->new({
            exec => [ 'sh', '-c', qq|DISPLAY=$display LOGPATH="$logpath" /usr/bin/perl -It/lib $test| ],
            spool => IO::Scalar->new(\$output),
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
