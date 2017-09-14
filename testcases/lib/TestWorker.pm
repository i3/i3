# vim:ts=4:sw=4:sts=4:expandtab
package TestWorker;
use strict; use warnings;
use v5.10;

use Socket qw(AF_UNIX SOCK_DGRAM PF_UNSPEC);
use IO::Handle; # for ->autoflush

use POSIX ();

use Errno qw(EAGAIN);

use Exporter 'import';
our @EXPORT = qw(worker worker_next);

use File::Basename qw(basename);
my @x;
my $options;

sub worker {
    my ($display, $x, $outdir, $optref) = @_;

    # make sure $x hangs around
    push @x, $x;

    # store the options hashref
    $options = $optref;

    socketpair(my $ipc_child, my $ipc, AF_UNIX, SOCK_DGRAM, PF_UNSPEC)
        or die "socketpair: $!";

    $ipc->autoflush(1);
    $ipc_child->autoflush(1);

    my $worker = {
        display => $display,
        ipc => $ipc,
    };

    my $pid = fork // die "could not fork: $!";

    if ($pid == 0) {
        close $ipc;
        undef @complete_run::CLEANUP;
        # reap dead test children
        $SIG{CHLD} = sub { waitpid -1, POSIX::WNOHANG };

        $worker->{ipc} = $ipc_child;

        # Preload the i3test module: reduces user CPU from 25s to 18s
        require i3test;

        worker_wait($worker, $outdir);
        exit 23;

    }

    close $ipc_child;
    push @complete_run::CLEANUP, sub {
        # signal via empty line to exit itself
        syswrite($ipc, "\n") or kill('TERM', $pid);
        waitpid $pid, 0;
    };

    return $worker;

}

our $EOF = "# end of file\n";
sub worker_wait {
    my ($self, $outdir) = @_;

    my $ipc = $self->{ipc};
    my $ipc_fd = fileno($ipc);

    while (1) {
        my $file = $ipc->getline;
        if (!defined($file)) {
            next if $! == EAGAIN;
            last;
        }
        chomp $file;

        exit unless $file;

        die "tried to launch nonexistent testfile $file: $!\n"
            unless -e $file;

        # start a new and self contained process:
        # whatever happens in the testfile should *NOT* affect us.

        my $pid = fork // die "could not fork: $!";
        if ($pid == 0) {
            undef @complete_run::CLEANUP;
            local $SIG{CHLD};

            $0 = $file;

            # Re-seed rand() so that File::Temp’s tempnam produces different
            # results, making a TOCTOU between e.g. t/175-startup-notification.t
            # and t/180-fd-leaks.t less likely.
            srand(time ^ $$);

            POSIX::dup2($ipc_fd, 0);
            POSIX::dup2($ipc_fd, 1);
            POSIX::dup2(1, 2);

            # get Test::Builder singleton
            my $test = Test::Builder->new;

            # Test::Builder dups stdout/stderr while loading.
            # we need to reset them here to point to $ipc
            $test->output(\*STDOUT);
            $test->failure_output(\*STDERR);
            $test->todo_output(\*STDOUT);

            @ENV{qw(HOME DISPLAY TESTNAME OUTDIR VALGRIND STRACE XTRACE COVERAGE RESTART)}
                = ($outdir,
                   $self->{display},
                   basename($file),
                   $outdir,
                   $options->{valgrind},
                   $options->{strace},
                   $options->{xtrace},
                   $options->{coverage},
                   $options->{restart});

            package main;
            local $@;
            do $file;
            $test->ok(undef, "$@") if $@;

            # XXX hack, we need to trigger the read watcher once more
            # to signal eof to TAP::Parser
            print $EOF;

            exit 0;
        }
    }
}

sub worker_next {
    my ($self, $file) = @_;

    my $ipc = $self->{ipc};
    syswrite $ipc, "$file\n" or die "syswrite: $!";
}

__PACKAGE__ __END__
