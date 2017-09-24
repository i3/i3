package StartXServer;
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use Exporter 'import';
use Time::HiRes qw(sleep);
use i3test::Util qw(slurp);
use v5.10;

our @EXPORT = qw(start_xserver);

my @pids;
my $x_socketpath = '/tmp/.X11-unix/X';

# forks an X server process
sub fork_xserver {
    my $keep_xserver_output = shift;
    my $displaynum = shift;
    my $pid = fork();
    die "Could not fork: $!" unless defined($pid);
    if ($pid == 0) {
        # Child, close stdout/stderr, then start Xephyr
        if (!$keep_xserver_output) {
            close STDOUT;
            close STDERR;
        }

        exec @_;
        exit 1;
    }
    push(@complete_run::CLEANUP, sub {
        kill(15, $pid);
        # Unlink the X11 socket, Xdmx seems to leave it there.
        unlink($x_socketpath . $displaynum);
    });

    push @pids, $pid;

    return $x_socketpath . $displaynum;
}

# Blocks until the socket paths specified in the given array reference actually
# exist.
sub wait_for_x {
    my ($sockets_waiting) = @_;

    # Wait until Xdmx actually runs. Pretty ugly solution, but as long as we
    # can’t socket-activate X11…
    while (1) {
        @$sockets_waiting = grep { ! -S $_ } @$sockets_waiting;
        last unless @$sockets_waiting;
        sleep 0.1;
    }
}

=head2 start_xserver($parallel)

Starts C<$parallel> (or number of cores * 2 if undef) Xephyr processes (see
https://www.freedesktop.org/wiki/Software/Xephyr/) and returns two arrayrefs: a
list of X11 display numbers to the Xephyr processes and a list of PIDs of the
processes.

=cut

sub start_xserver {
    my ($parallel, $numtests, $keep_xserver_output) = @_;

    my @displays = ();
    my @childpids = ();

    # Yeah, I know it’s non-standard, but Perl’s POSIX module doesn’t have
    # _SC_NPROCESSORS_CONF.
    my $num_cores;
    if (-e '/proc/cpuinfo') {
        my $cpuinfo = slurp('/proc/cpuinfo');
        $num_cores = scalar grep { /model name/ } split("\n", $cpuinfo);
    }
    # If /proc/cpuinfo does not exist, we fall back to 2 cores.
    $num_cores ||= 2;

    # If unset, we use num_cores * 2.
    $parallel ||= ($num_cores * 2);

    # If we are running a small number of tests, don’t over-parallelize.
    $parallel = $numtests if $numtests < $parallel;

    # First get the last used display number, then increment it by one.
    # Effectively falls back to 1 if no X server is running.
    my ($displaynum) = reverse sort { $a <=> $b } map{ /(\d+)$/ } glob($x_socketpath . '*');
    $displaynum++;

    say "Starting $parallel Xephyr instances, starting at :$displaynum...";

    $SIG{CHLD} = sub {
        my $child = waitpid -1, POSIX::WNOHANG;
        @pids = grep { $_ != $child } @pids;
        return unless @pids == 0;
        print STDERR "All X server processes died.\n";
        print STDERR "Use ./complete-run.pl --parallel 1 --keep-xserver-output\n";
        exit 1;
    };

    my @sockets_waiting;
    for (1 .. $parallel) {
        my $socket = fork_xserver($keep_xserver_output, $displaynum,
                'Xephyr', ":$displaynum", '-screen', '1280x800',
                '-nolisten', 'tcp', '-name', "i3test");
        push(@displays, ":$displaynum");
        push(@sockets_waiting, $socket);
        $displaynum++;
    }

    wait_for_x(\@sockets_waiting);

    return @displays;
}

1
