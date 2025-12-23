#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • https://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • https://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • https://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Verify that i3bar only shows correct workspace buttons in each output.
# Ticket: #6560
# Bug still in: 4.25-6-g0e2e8290
use i3test i3_autostart => 0;
use File::Temp qw(tempfile tempdir);
use POSIX qw(mkfifo);
use i3test::XTEST;

################################################################################
# Test that a bar configured for primary output only shows workspaces from that
# output, not from other outputs.
################################################################################

# Create temp files for i3bar PID and exit signaling
my $tmpdir = tempdir(CLEANUP => 1);
my $pidfile = "$tmpdir/i3bar.pid";
my $exitfifo = "$tmpdir/fifo";
mkfifo("$exitfifo", 0600) or BAIL_OUT "Could not create FIFO: $!";

# Create a wrapper script that tracks i3bar's PID and signals when it exits
my ($scriptfh, $scriptfile) = tempfile(SUFFIX => '.sh', UNLINK => 1);
print $scriptfh <<"EOF";
#!/bin/sh
i3bar -V "\$@" 2>&1 &
echo \$! > "$pidfile"
wait
echo done > "$exitfifo"
EOF
close($scriptfh);
chmod 0755, $scriptfile;

my $config = <<"EOT";
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0P,1024x768+1024+0
workspace 1 output fake-1 # primary
workspace 2 output fake-0 # nonprimary

bar {
    i3bar_command $scriptfile
    output primary
}
EOT

my $pid = launch_with_config($config);

my $i3 = i3(get_socket_path());
$i3->connect()->recv;
my $cv = AnyEvent->condvar;
my $timer = AnyEvent->timer(after => 1, interval => 0, cb => sub { $cv->send(0) });
$i3->subscribe({
        window => sub {
            my ($event) = @_;
            if ($event->{change} eq 'new') {
                if (defined($event->{container}->{window_properties}->{class}) &&
                    $event->{container}->{window_properties}->{class} eq 'i3bar') {
                    $cv->send($event->{container});
                }
            }
        },
    })->recv;

sub i3bar_present {
    my ($nodes) = @_;

    for my $node (@{$nodes}) {
	my $props = $node->{window_properties};
	if (defined($props) && $props->{class} eq 'i3bar') {
	    return $node->{window};
	}
    }

    return 0 if !@{$nodes};

    my @children = (map { @{$_->{nodes}} } @{$nodes},
                    map { @{$_->{'floating_nodes'}} } @{$nodes});

    return i3bar_present(\@children);
}

my $i3bar_window = i3bar_present($i3->get_tree->recv->{nodes});
if ($i3bar_window) {
    ok(1, 'i3bar present');
} else {
    my $con = $cv->recv;
    ok($con, 'i3bar appeared');
    $i3bar_window = $con->{window};
}

diag('i3bar window = ' . $i3bar_window);
xtest_sync_with_i3;
xtest_sync_with($i3bar_window);

# The actual test
cmd 'workspace 1';
my $win1 = open_window;
cmd 'workspace 2';
my $win2 = open_window;

# Kill i3bar gracefully BEFORE exiting i3 to ensure buffers are flushed
# (if i3 exits first, i3bar gets SIGPIPE and buffers are lost)
open(my $pidfh, '<', $pidfile) or BAIL_OUT "Cannot read i3bar PID: $!";
my $bar_pid = <$pidfh>;
close($pidfh);
chomp($bar_pid);
kill('TERM', $bar_pid);

# Wait for i3bar to exit by reading from the FIFO (blocks until wrapper writes)
open(my $fifofh, '<', $exitfifo) or BAIL_OUT "Cannot open FIFO: $!";
my $result = <$fifofh>;
close($fifofh);
ok(defined($result), 'i3bar ended');

exit_gracefully($pid);

my $log = get_i3_log;

my @ws2_draws = ($log =~ /Drawing button for WS 2 at/g);
ok(scalar(@ws2_draws) > 0, "Workspace 2 (on primary) is drawn in the bar");

my @ws1_draws = ($log =~ /Drawing button for WS 1 at/g);
is(scalar(@ws1_draws), 0, "Workspace 1 (on non-primary) should NOT be drawn on primary bar");

done_testing;
