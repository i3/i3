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

use i3test;

SKIP: {

    skip "AnyEvent::I3 too old (need >= 0.15)", 1 if $AnyEvent::I3::VERSION < 0.15;

my $i3 = i3(get_socket_path());
$i3->connect()->recv;

################################
# Window event
################################

# Events

my $new = AnyEvent->condvar;
$i3->subscribe({
    window => sub {
        my ($event) = @_;
        $new->send($event->{change} eq 'new');
    }
})->recv;

open_window;

my $t;
$t = AnyEvent->timer(after => 0.5, cb => sub { $new->send(0); });

ok($new->recv, 'Window "new" event received');

}

done_testing;
