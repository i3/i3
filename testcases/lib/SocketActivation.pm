package SocketActivation;
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use IO::Socket::UNIX; # core
use Cwd qw(abs_path); # core
use POSIX qw(:fcntl_h); # core
use AnyEvent::Handle; # not core
use AnyEvent::Util; # not core
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
        delete $ENV{I3SOCK};
        # $SHELL could be set to fish, which will horribly break running shell
        # commands via i3’s exec feature. This happened e.g. when having
        # “set-option -g default-shell "/usr/bin/fish"” in ~/.tmux.conf
        delete $ENV{SHELL};
        unless ($args{dont_create_temp_dir}) {
            $ENV{XDG_RUNTIME_DIR} = '/tmp/i3-testsuite/';
            mkdir $ENV{XDG_RUNTIME_DIR};
        }
        $ENV{DISPLAY} = $args{display};
        $ENV{PATH} = join(':',
            '../i3-nagbar',
            '../i3-msg',
            '../i3-config-wizard',
            '../i3bar',
            '..',
            $ENV{PATH}
        );

        # We are about to exec, but we did not modify $^F to include $socket
        # when creating the socket (because the file descriptor could have a
        # number != 3 which would lead to i3 leaking a file descriptor). This
        # caused Perl to set the FD_CLOEXEC flag, which would close $socket on
        # exec(), effectively *NOT* passing $socket to the new process.
        # Therefore, we explicitly clear FD_CLOEXEC (the only flag right now)
        # by setting the flags to 0.
        POSIX::fcntl($socket, F_SETFD, 0) or die "Could not clear fd flags: $!";

        # If the socket does not use file descriptor 3 by chance already, we
        # close fd 3 and dup2() the socket to 3.
        if (fileno($socket) != 3) {
            POSIX::close(3);
            POSIX::dup2(fileno($socket), 3);
            POSIX::close(fileno($socket));
        }

        # Make sure no file descriptors are open. Strangely, I got an open file
        # descriptor pointing to AnyEvent/Impl/EV.pm when testing.
        AnyEvent::Util::close_all_fds_except(0, 1, 2, 3);

        # Construct the command to launch i3. Use maximum debug level, disable
        # the interactive signalhandler to make it crash immediately instead.
        # Also disable logging to SHM since we redirect the logs anyways.
        # Force Xinerama because we use Xdmx for multi-monitor tests.
        my $i3cmd = abs_path("../i3") . q| -V -d all --disable-signalhandler| .
                                        q| --shmlog-size=0 --force-xinerama|;

        # For convenience:
        my $outdir = $args{outdir};
        my $test = $args{testname};

        if ($args{restart}) {
            $i3cmd .= ' -L ' . abs_path('restart-state.golden');
        }

        if ($args{valgrind}) {
            $i3cmd =
                qq|valgrind --log-file="$outdir/valgrind-for-$test.log" | .
                qq|--suppressions="./valgrind.supp" | .
                qq|--leak-check=full --track-origins=yes --num-callers=20 | .
                qq|--tool=memcheck -- $i3cmd|;
        }

        my $logfile = "$outdir/i3-log-for-$test";
        # Append to $logfile instead of overwriting because i3 might be
        # run multiple times in one testcase.
        my $cmd = "exec $i3cmd -c $args{configfile} >>$logfile 2>&1";

        if ($args{strace}) {
            my $out = "$outdir/strace-for-$test.log";

            # We overwrite LISTEN_PID with the correct process ID to make
            # socket activation work (LISTEN_PID has to match getpid(),
            # otherwise the LISTEN_FDS will be treated as a left-over).
            $cmd = qq|strace -fF -s2048 -v -o "$out" -- | .
                     'sh -c "export LISTEN_PID=\$\$; ' . $cmd . '"';
        }

        if ($args{xtrace}) {
            my $out = "$outdir/xtrace-for-$test.log";

            # See comment in $args{strace} branch.
            $cmd = qq|xtrace -n -o "$out" -- | .
                     'sh -c "export LISTEN_PID=\$\$; ' . $cmd . '"';
        }

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
