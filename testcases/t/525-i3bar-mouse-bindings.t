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
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Ensures that mouse bindings on the i3bar work correctly.
# Ticket: #1695
use i3test i3_config => <<EOT;
# i3 config file (v4)
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
}
EOT
use i3test::XTEST;

my $i3 = i3(get_socket_path());
$i3->connect()->recv;
my $ws = fresh_workspace;

my $cv = AnyEvent->condvar;
my $timer = AnyEvent->timer(1, 0, sub { $cv->send(0) });
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
	    return 1;
	}
    }

    return 0 if !@{$nodes};

    my @children = (map { @{$_->{nodes}} } @{$nodes},
                    map { @{$_->{'floating_nodes'}} } @{$nodes});

    return i3bar_present(\@children);
}

if (i3bar_present($i3->get_tree->recv->{nodes})) {
    ok(1, 'i3bar present');
} else {
    my $con = $cv->recv;
    ok($con, 'i3bar appeared');
}

my $left = open_window;
my $right = open_window;
sync_with_i3;
my $con = $cv->recv;
is($con->{window}, $right->{id}, 'focus is initially on the right container');

sub focus_subtest {
    my ($subscribecb, $want, $msg) = @_;
    my @events = events_for(
	$subscribecb,
	'window');
    my @focus = map { $_->{container}->{window} } grep { $_->{change} eq 'focus' } @events;
    is_deeply(\@focus, $want, $msg);
}

subtest 'button 1 moves focus left', \&focus_subtest,
    sub {
	xtest_button_press(1, 3, 3);
	xtest_button_release(1, 3, 3);
	xtest_sync_with_i3;
    },
    [ $left->{id} ],
    'button 1 moves focus left';

subtest 'button 2 moves focus right', \&focus_subtest,
    sub {
	xtest_button_press(2, 3, 3);
	xtest_button_release(2, 3, 3);
	xtest_sync_with_i3;
    },
    [ $right->{id} ],
    'button 2 moves focus right';

subtest 'button 3 moves focus left', \&focus_subtest,
    sub {
	xtest_button_press(3, 3, 3);
	xtest_button_release(3, 3, 3);
	xtest_sync_with_i3;
    },
    [ $left->{id} ],
    'button 3 moves focus left';

subtest 'button 4 moves focus right', \&focus_subtest,
    sub {
	xtest_button_press(4, 3, 3);
	xtest_button_release(4, 3, 3);
	xtest_sync_with_i3;
    },
    [ $right->{id} ],
    'button 4 moves focus right';

subtest 'button 5 moves focus left', \&focus_subtest,
    sub {
	xtest_button_press(5, 3, 3);
	xtest_button_release(5, 3, 3);
	xtest_sync_with_i3;
    },
    [ $left->{id} ],
    'button 5 moves focus left';

done_testing;
