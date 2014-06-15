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
# Tests that the ipc window::fullscreen_mode event works properly
#
# Bug still in: 4.7.2-135-g7deb23c
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
            if ($e->{change} eq 'fullscreen_mode') {
                $cv->send($e->{container});
            }
        },
    })->recv;

my $window = open_window;

cmd 'fullscreen';
my $con = $cv->recv;

ok($con, 'got fullscreen window event (on)');
is($con->{fullscreen_mode}, 1, 'window is fullscreen');

reset_test;
cmd 'fullscreen';
$con = $cv->recv;

ok($con, 'got fullscreen window event (off)');
is($con->{fullscreen_mode}, 0, 'window is not fullscreen');

done_testing;
