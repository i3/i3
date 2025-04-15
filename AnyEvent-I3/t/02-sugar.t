#!perl
# vim:ts=4:sw=4:expandtab

use Test::More tests => 3;
use AnyEvent::I3;
use AnyEvent;

my $i3 = i3();
my $cv = AnyEvent->condvar;

# Try to connect to i3
$i3->connect->cb(sub { my ($v) = @_; $cv->send($v->recv) });

# But cancel if we are not connected after 0.5 seconds
my $t = AnyEvent->timer(after => 0.5, cb => sub { $cv->send(0) });
my $connected = $cv->recv;

SKIP: {
    skip 'No connection to i3', 3 unless $connected;

    my $workspaces = i3->get_workspaces->recv;
    isa_ok($workspaces, 'ARRAY');

    ok(@{$workspaces} > 0, 'More than zero workspaces found');

    ok(defined(@{$workspaces}[0]->{num}), 'JSON deserialized');
}

diag( "Testing AnyEvent::I3 $AnyEvent::I3::VERSION, Perl $], $^X" );
