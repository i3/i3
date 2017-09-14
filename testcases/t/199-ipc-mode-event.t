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
# Verifies that the IPC 'mode' event is sent when modes are changed.
use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

mode "m1" {
    bindsym Mod1+x nop foo
}

mode "with spaces" {
    bindsym Mod1+y nop bar
}
EOT

my $i3 = i3(get_socket_path(0));
$i3->connect->recv;

my $cv = AnyEvent->condvar;

$i3->subscribe({
    mode => sub {
        my ($event) = @_;
        $cv->send($event->{change} eq 'm1');
    }
})->recv;

cmd 'mode "m1"';

# Timeout after 0.5s
my $t;
$t = AnyEvent->timer(after => 0.5, cb => sub { $cv->send(0); });

ok($cv->recv, 'Mode event received');

done_testing;
