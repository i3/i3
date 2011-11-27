package StartXDummy;
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use Exporter 'import';
use Time::HiRes qw(sleep);
use v5.10;

our @EXPORT = qw(start_xdummy);

# reads in a whole file
sub slurp {
    open(my $fh, '<', shift) or return '';
    local $/;
    <$fh>;
}

=head2 start_xdummy($parallel)

Starts C<$parallel> (or number of cores * 2 if undef) Xdummy processes (see
the file ./Xdummy) and returns two arrayrefs: a list of X11 display numbers to
the Xdummy processes and a list of PIDs of the processes.

=cut

my $x_socketpath = '/tmp/.X11-unix/X';

sub start_xdummy {
    my ($parallel) = @_;

    my @displays = ();
    my @childpids = ();

    # Yeah, I know it’s non-standard, but Perl’s POSIX module doesn’t have
    # _SC_NPROCESSORS_CONF.
    my $cpuinfo = slurp('/proc/cpuinfo');
    my $num_cores = scalar grep { /model name/ } split("\n", $cpuinfo);
    # If /proc/cpuinfo does not exist, we fall back to 2 cores.
    $num_cores ||= 2;

    $parallel ||= $num_cores * 2;

    # First get the last used display number, then increment it by one.
    # Effectively falls back to 1 if no X server is running.
    my ($displaynum) = map { /(\d+)$/ } reverse sort glob($x_socketpath . '*');
    $displaynum++;

    say "Starting $parallel Xdummy instances, starting at :$displaynum...";

    my @sockets_waiting;
    for my $idx (0 .. ($parallel-1)) {
        my $pid = fork();
        die "Could not fork: $!" unless defined($pid);
        if ($pid == 0) {
            # Child, close stdout/stderr, then start Xdummy.
            close STDOUT;
            close STDERR;
            # make sure this display isn’t in use yet
            $displaynum++ while -e ($x_socketpath . $displaynum);

            # We use -config /dev/null to prevent Xdummy from using the system
            # Xorg configuration. The tests should be independant from the
            # actual system X configuration.
            exec './Xdummy', ":$displaynum", '-config', '/dev/null';
            exit 1;
        }
        push(@main::CLEANUP, sub { kill(15, $pid) });
        push(@displays, ":$displaynum");
        push(@sockets_waiting, $x_socketpath . $displaynum);
        $displaynum++;
    }

    # Wait until the X11 sockets actually appear. Pretty ugly solution, but as
    # long as we can’t socket-activate X11…
    while (1) {
        @sockets_waiting = grep { ! -S $_ } @sockets_waiting;
        last unless @sockets_waiting;
        sleep 0.1;
    }

    return @displays;
}

1
