package StartXDummy;
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use POSIX ();
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
    my ($displaynum) = reverse ('0', sort </tmp/.X11-unix/X*>);
    $displaynum =~ s/.*(\d)$/$1/;
    $displaynum++;

    say "Starting $parallel Xdummy instances, starting at :$displaynum...";

    for my $idx (0 .. ($parallel-1)) {
        my $pid = fork();
        die "Could not fork: $!" unless defined($pid);
        if ($pid == 0) {
            # Child, close stdout/stderr, then start Xdummy.
            POSIX::close(0);
            POSIX::close(2);
            exec './Xdummy', ":$displaynum", '-config', '/dev/null';
            exit 1;
        }
        push(@childpids, $pid);
        push(@displays, ":$displaynum");
        $displaynum++;
    }

    # Wait until the X11 sockets actually appear. Pretty ugly solution, but as
    # long as we can’t socket-activate X11…
    my $sockets_ready;
    do {
        $sockets_ready = 1;
        for (@displays) {
            my $path = "/tmp/.X11-unix/X" . substr($_, 1);
            $sockets_ready = 0 unless -S $path;
        }
        sleep 0.1;
    } until $sockets_ready;

    return \@displays, \@childpids;
}

1
