#!perl
# vim:ts=4:sw=4:expandtab
#
# checks if i3 supports I3_SYNC
#
use X11::XCB qw(:all);
use X11::XCB::Connection;
use i3test;

my $x = X11::XCB::Connection->new;

my $result = sync_with_i3($x);
ok($result, 'syncing was successful');

done_testing;
