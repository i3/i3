#!perl
# vim:ts=4:sw=4:expandtab
#
# checks if i3 supports I3_SYNC
#
use i3test;

my $result = sync_with_i3;
ok($result, 'syncing was successful');

done_testing;
