package StartXDummy;
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use Exporter 'import';
use Time::HiRes qw(sleep);
use v5.10;

our @EXPORT = qw(start_xdummy);

my $x_socketpath = '/tmp/.X11-unix/X';

# reads in a whole file
sub slurp {
    open(my $fh, '<', shift) or return '';
    local $/;
    <$fh>;
}

# forks an Xdummy or Xdmx process
sub fork_xserver {
    my $displaynum = shift;
    my $pid = fork();
    die "Could not fork: $!" unless defined($pid);
    if ($pid == 0) {
        # Child, close stdout/stderr, then start Xdummy.
        close STDOUT;
        close STDERR;

        exec @_;
        exit 1;
    }
    push(@complete_run::CLEANUP, sub {
        kill(15, $pid);
        # Unlink the X11 socket, Xdmx seems to leave it there.
        unlink($x_socketpath . $displaynum);
    });

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

=head2 start_xdummy($parallel)

Starts C<$parallel> (or number of cores * 2 if undef) Xdummy processes (see
the file ./Xdummy) and returns two arrayrefs: a list of X11 display numbers to
the Xdummy processes and a list of PIDs of the processes.

=cut

sub start_xdummy {
    my ($parallel, $numtests) = @_;

    my @displays = ();
    my @childpids = ();

    # Yeah, I know it’s non-standard, but Perl’s POSIX module doesn’t have
    # _SC_NPROCESSORS_CONF.
    my $cpuinfo = slurp('/proc/cpuinfo');
    my $num_cores = scalar grep { /model name/ } split("\n", $cpuinfo);
    # If /proc/cpuinfo does not exist, we fall back to 2 cores.
    $num_cores ||= 2;

    # If unset, we use num_cores * 2, plus two extra xdummys to combine to a
    # multi-monitor setup using Xdmx.
    $parallel ||= ($num_cores * 2) + 2;

    # If we are running a small number of tests, don’t over-parallelize.
    $parallel = $numtests if $numtests < $parallel;

    # Ensure we have at least 1 X-Server plus two X-Servers for multi-monitor
    # tests.
    $parallel = 3 if $parallel < 3;

    # First get the last used display number, then increment it by one.
    # Effectively falls back to 1 if no X server is running.
    my ($displaynum) = map { /(\d+)$/ } reverse sort glob($x_socketpath . '*');
    $displaynum++;

    say "Starting $parallel Xdummy instances, starting at :$displaynum...";

    my @sockets_waiting;
    for (1 .. $parallel) {
        # We use -config /dev/null to prevent Xdummy from using the system
        # Xorg configuration. The tests should be independant from the
        # actual system X configuration.
        my $socket = fork_xserver($displaynum, './Xdummy', ":$displaynum",
                '-config', '/dev/null');
        push(@displays, ":$displaynum");
        push(@sockets_waiting, $socket);
        $displaynum++;
    }

    wait_for_x(\@sockets_waiting);

    # Now combine the last two displays to a multi-monitor display using Xdmx
    my $first = pop @displays;
    my $second = pop @displays;

    # make sure this display isn’t in use yet
    $displaynum++ while -e ($x_socketpath . $displaynum);
    say 'starting xdmx on display :' . $displaynum;

    my $multidpy = ":$displaynum";
    my $socket = fork_xserver($displaynum, 'Xdmx', '+xinerama', '-xinput',
            'local', '-display', $first, '-display', $second, '-ac', $multidpy);
    wait_for_x([ $socket ]);

    return \@displays, $multidpy;
}

1
