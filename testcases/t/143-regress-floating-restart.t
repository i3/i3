#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression: floating windows are tiling after restarting, closing them crashes i3
#
use i3test;

my $tmp = fresh_workspace;

cmd 'open';
cmd 'floating toggle';

my $ws = get_ws($tmp);
is(scalar @{$ws->{nodes}}, 0, 'no tiling nodes');
is(scalar @{$ws->{floating_nodes}}, 1, 'precisely one floating node');

cmd 'restart';
sleep 0.5;

diag('Checking if i3 still lives');

does_i3_live;

$ws = get_ws($tmp);
is(scalar @{$ws->{nodes}}, 0, 'no tiling nodes');
is(scalar @{$ws->{floating_nodes}}, 1, 'precisely one floating node');

done_testing;
