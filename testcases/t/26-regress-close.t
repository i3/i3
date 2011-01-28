#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression: closing of floating clients did crash i3 when closing the
# container which contained this client.
#
use i3test tests => 1;
use X11::XCB qw(:all);

my $tmp = get_unused_workspace();
cmd "workspace $tmp";

cmd 'open';
cmd 'mode toggle';
cmd 'kill';
cmd 'kill';

does_i3_live;

diag( "Testing i3, Perl $], $^X" );
