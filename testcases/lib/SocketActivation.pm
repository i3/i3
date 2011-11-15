package SocketActivation;
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use IO::Socket::UNIX; # core
use Cwd qw(abs_path); # core
use POSIX (); # core
use AnyEvent::Handle; # not core
use Exporter 'import';
use v5.10;

our @EXPORT = qw(activate_i3);

#
# Starts i3 using socket activation. Creates a listening socket (with bind +
# listen) which is then passed to i3, who in turn calls accept and handles the
# requests.
#
# Since the kernel buffers the connect, the parent process can connect to the
# socket immediately after forking. It then sends a request and waits until it
# gets an answer. Obviously, i3 has to be initialized to actually answer the
# request.
#
# This way, we can wait *precisely* the amount of time which i3 waits to get
# ready, which is a *HUGE* speed gain (and a lot more robust) in comparison to
# using sleep() with a fixed amount of time.
#
# unix_socket_path: Location of the socket to use for the activation
# display: X11 $ENV{DISPLAY}
# configfile: path to the configuration file to use
# logpath: path to the logfile to which i3 will append
# cv: an AnyEvent->condvar which will be triggered once i3 is ready
#
sub activate_i3 {
    my %args = @_;

    # remove the old unix socket
    unlink($args{unix_socket_path});

    my $socket = IO::Socket::UNIX->new(
        Listen => 1,
        Local => $args{unix_socket_path},
    );

    my $pid = fork;
    if (!defined($pid)) {
        die "could not fork()";
    }
    if ($pid == 0) {
        $ENV{LISTEN_PID} = $$;
        $ENV{LISTEN_FDS} = 1;
        delete $ENV{DESKTOP_STARTUP_ID};
        $ENV{DISPLAY} = $args{display};
        $ENV{PATH} = join(':',
            '../i3-nagbar',
            '../i3-msg',
            '../i3-config-wizard',
            '../i3bar',
            '..',
            $ENV{PATH}
        );
        # Only pass file descriptors 0 (stdin), 1 (stdout), 2 (stderr) and
        # 3 (socket) to the child.
        $^F = 3;

        # If the socket does not use file descriptor 3 by chance already, we
        # close fd 3 and dup2() the socket to 3.
        if (fileno($socket) != 3) {
            POSIX::close(3);
            POSIX::dup2(fileno($socket), 3);
        }

        # Construct the command to launch i3. Use maximum debug level, disable
        # the interactive signalhandler to make it crash immediately instead.
        my $i3cmd = abs_path("../i3") . " -V -d all --disable-signalhandler";

        if ($args{valgrind}) {
            $i3cmd =
                qq|valgrind -v --log-file="$args{outdir}/valgrind.log" | .
                qq|--leak-check=full --track-origins=yes --num-callers=20 | .
                qq|--tool=memcheck -- $i3cmd|;
        }

        # Append to $args{logpath} instead of overwriting because i3 might be
        # run multiple times in one testcase.
        my $cmd = "exec $i3cmd -c $args{configfile} >>$args{logpath} 2>&1";

        # We need to use the shell due to using output redirections.
        exec '/bin/sh', '-c', $cmd;

        # if we are still here, i3 could not be found or exec failed. bail out.
        exit 1;
    }

    # close the socket, the child process should be the only one which keeps a file
    # descriptor on the listening socket.
    $socket->close;

    # We now connect (will succeed immediately) and send a request afterwards.
    # As soon as the reply is there, i3 is considered ready.
    my $cl = IO::Socket::UNIX->new(Peer => $args{unix_socket_path});
    my $hdl;
    $hdl = AnyEvent::Handle->new(
        fh => $cl,
        on_error => sub {
            $hdl->destroy;
            $args{cv}->send(0);
        });

    # send a get_tree message without payload
    $hdl->push_write('i3-ipc' . pack("LL", 0, 4));

    # wait for the reply
    $hdl->push_read(chunk => 1, => sub {
        my ($h, $line) = @_;
        $args{cv}->send(1);
        undef $hdl;
    });

    return $pid;
}

1
