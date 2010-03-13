#!perl -T

use Test::More tests => 1;
use AnyEvent::I3;

my $i3 = i3();
my $cv = $i3->connect;
$cv->recv;

ok(1, "connected");

diag( "Testing AnyEvent::I3 $AnyEvent::I3::VERSION, Perl $], $^X" );
