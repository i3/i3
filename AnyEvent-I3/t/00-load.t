#!perl

use Test::More tests => 1;

BEGIN {
    use_ok( 'AnyEvent::I3' ) || print "Bail out!
";
}

diag( "Testing AnyEvent::I3 $AnyEvent::I3::VERSION, Perl $], $^X" );
