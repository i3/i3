#!perl
# vim:ts=4:sw=4:expandtab
#
# 
#
use i3test tests => 1;

cmd 'blargh!';

does_i3_live;

diag( "Testing i3, Perl $], $^X" );
