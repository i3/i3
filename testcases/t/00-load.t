#!perl

use Test::More tests => 2;

BEGIN {
	use_ok( 'X11::XCB::Connection' );
	use_ok( 'X11::XCB::Window' );
}

diag( "Testing i3, Perl $], $^X" );
