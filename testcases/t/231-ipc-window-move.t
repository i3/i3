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
# Tests that the ipc window::move event works properly
#
# Bug still in: 4.8-7-gf4a8253
use i3test;

my $i3 = i3(get_socket_path());
$i3->connect()->recv;

my $cv;
my $t;

sub reset_test {
    $cv = AE::cv;
    $t = AE::timer(0.5, 0, sub { $cv->send(0); });
}

reset_test;

$i3->subscribe({
        window => sub {
            my ($e) = @_;
            if ($e->{change} eq 'move') {
                $cv->send($e->{container});
            }
        },
    })->recv;

my $dummy_window = open_window;
my $window = open_window;

cmd 'move right';
my $con = $cv->recv;

ok($con, 'moving a window should emit the window::move event');
is($con->{window}, $window->{id}, 'the event should contain info about the window');

reset_test;

cmd 'move to workspace ws_new';
$con = $cv->recv;

ok($con, 'moving a window to a different workspace should emit the window::move event');
is($con->{window}, $window->{id}, 'the event should contain info about the window');

done_testing;
