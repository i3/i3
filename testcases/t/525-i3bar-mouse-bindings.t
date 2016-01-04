#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Ensures that mouse bindings on the i3bar work correctly.
# Ticket: #1695
use i3test i3_autostart => 0;
use i3test::XTEST;

my ($cv, $timer);
sub reset_test {
    $cv = AE::cv;
    $timer = AE::timer(1, 0, sub { $cv->send(0); });
}

my $config = <<EOT;
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

my $pid = launch_with_config($config);
my $i3 = i3(get_socket_path());
$i3->connect()->recv;
my $ws = fresh_workspace;

reset_test;
$i3->subscribe({
        window => sub {
            my ($event) = @_;
            if ($event->{change} eq 'focus') {
                $cv->send($event->{container});
            }
        },
    })->recv;

my $left = open_window;
my $right = open_window;
sync_with_i3;
my $con = $cv->recv;
is($con->{window}, $right->{id}, 'focus is initially on the right container');
reset_test;

xtest_button_press(1, 3, 3);
xtest_button_release(1, 3, 3);
sync_with_i3;
$con = $cv->recv;
is($con->{window}, $left->{id}, 'button 1 moves focus left');
reset_test;

xtest_button_press(2, 3, 3);
xtest_button_release(2, 3, 3);
sync_with_i3;
$con = $cv->recv;
is($con->{window}, $right->{id}, 'button 2 moves focus right');
reset_test;

xtest_button_press(3, 3, 3);
xtest_button_release(3, 3, 3);
sync_with_i3;
$con = $cv->recv;
is($con->{window}, $left->{id}, 'button 3 moves focus left');
reset_test;

xtest_button_press(4, 3, 3);
xtest_button_release(4, 3, 3);
sync_with_i3;
$con = $cv->recv;
is($con->{window}, $right->{id}, 'button 4 moves focus right');
reset_test;

xtest_button_press(5, 3, 3);
xtest_button_release(5, 3, 3);
sync_with_i3;
$con = $cv->recv;
is($con->{window}, $left->{id}, 'button 5 moves focus left');
reset_test;

exit_gracefully($pid);

done_testing;
