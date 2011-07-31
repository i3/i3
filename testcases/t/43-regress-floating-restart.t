#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression: floating windows are tiling after restarting, closing them crashes i3
#
use i3test;

my $tmp = fresh_workspace;

cmd 'open';
cmd 'mode toggle';
cmd 'restart';

sleep 0.5;

diag('Checking if i3 still lives');

does_i3_live;

my $ws = get_ws($tmp);
diag('ws = ' . Dumper($ws));

done_testing;
