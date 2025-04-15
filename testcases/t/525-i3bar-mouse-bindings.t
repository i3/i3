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
# Ensures that mouse bindings on the i3bar work correctly.
# Ticket: #1695
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
focus_follows_mouse no

bar {
    font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
    position top

    bindsym button1 focus left
    bindsym button2 focus right
    bindsym button3 focus left
    bindsym button4 focus right
    bindsym button5 focus left
    bindsym --release button6 focus right
    bindsym button7 focus left
    bindsym button7 --release focus right
}
EOT
use i3test::XTEST;

my $i3 = i3(get_socket_path());
$i3->connect()->recv;
my $ws = fresh_workspace;

my $cv = AnyEvent->condvar;
my $timer = AnyEvent->timer(after => 1, interval => 0, cb => sub { $cv->send(0) });
$i3->subscribe({
        window => sub {
            my ($event) = @_;
            if ($event->{change} eq 'focus') {
                $cv->send($event->{container});
            }
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

my $left = open_window;
my $right = open_window;
sync_with_i3;

sub focus_subtest {
    my ($subscribecb, $want, $msg) = @_;
    my @events = events_for(
	$subscribecb,
	'window');
    my @focus = map { $_->{container}->{window} } grep { $_->{change} eq 'focus' } @events;
    is_deeply(\@focus, $want, $msg);
}

sub sync {
    # Ensure XTEST events were sent to i3, which grabs and hence needs to
    # forward any events to i3bar:
    xtest_sync_with_i3;
    # Ensure any pending i3bar IPC messages were handled by i3:
    xtest_sync_with($i3bar_window);
}

subtest 'button 1 moves focus left', \&focus_subtest,
    sub {
	xtest_button_press(1, 3, 3);
	xtest_button_release(1, 3, 3);
	sync;
    },
    [ $left->{id} ],
    'button 1 moves focus left';

subtest 'button 2 moves focus right', \&focus_subtest,
    sub {
	xtest_button_press(2, 3, 3);
	xtest_button_release(2, 3, 3);
	sync;
    },
    [ $right->{id} ],
    'button 2 moves focus right';

subtest 'button 3 moves focus left', \&focus_subtest,
    sub {
	xtest_button_press(3, 3, 3);
	xtest_button_release(3, 3, 3);
	sync;
    },
    [ $left->{id} ],
    'button 3 moves focus left';

subtest 'button 4 moves focus right', \&focus_subtest,
    sub {
	xtest_button_press(4, 3, 3);
	xtest_button_release(4, 3, 3);
	sync;
    },
    [ $right->{id} ],
    'button 4 moves focus right';

subtest 'button 5 moves focus left', \&focus_subtest,
    sub {
	xtest_button_press(5, 3, 3);
	xtest_button_release(5, 3, 3);
	sync;
    },
    [ $left->{id} ],
    'button 5 moves focus left';

# Test --release flag with bar bindsym.
# See issue: #3068.

my $old_focus = get_focused($ws);
subtest 'button 6 does not move focus while pressed', \&focus_subtest,
    sub {
        xtest_button_press(6, 3, 3);
        sync;
    },
    [],
    'button 6 does not move focus while pressed';
is(get_focused($ws), $old_focus, 'focus unchanged');

subtest 'button 6 release moves focus right', \&focus_subtest,
    sub {
        xtest_button_release(6, 3, 3);
        sync;
    },
    [ $right->{id} ],
    'button 6 release moves focus right';

# Test same bindsym button with and without --release.

subtest 'button 7 press moves focus left', \&focus_subtest,
    sub {
        xtest_button_press(7, 3, 3);
        sync;
    },
    [ $left->{id} ],
    'button 7 press moves focus left';

subtest 'button 7 release moves focus right', \&focus_subtest,
    sub {
        xtest_button_release(7, 3, 3);
        sync;
    },
    [ $right->{id} ],
    'button 7 release moves focus right';

done_testing;
