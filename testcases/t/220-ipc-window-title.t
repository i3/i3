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

use i3test;

SKIP: {

    skip "AnyEvent::I3 too old (need >= 0.15)", 1 if $AnyEvent::I3::VERSION < 0.15;

my $i3 = i3(get_socket_path());
$i3->connect()->recv;

################################
# Window title event
################################

my $window = open_window(name => 'Window 0');

my $title = AnyEvent->condvar;

$i3->subscribe({
    window => sub {
        my ($event) = @_;
        $title->send($event);
    }
})->recv;

$window->name('New Window Title');

my $t;
$t = AnyEvent->timer(
    after => 0.5,
    cb => sub {
        $title->send(0);
    }
);

my $event = $title->recv;
is($event->{change}, 'title', 'Window title change event received');
is($event->{container}->{name}, 'New Window Title', 'Window title changed');

}

done_testing;
