#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression: when resizing two containers on a workspace, opening a floating
# client, then closing it again, i3 will re-distribute the space on the
# workspace as if a tiling container was closed, leading to the containers
# taking much more space than they possibly could.
#
use i3test;
use List::Util qw(sum);

my $tmp = fresh_workspace;

cmd 'exec /usr/bin/urxvt';
sleep 0.5;
cmd 'exec /usr/bin/urxvt';
sleep 0.5;
my ($nodes, $focus) = get_ws_content($tmp);
my $old_sum = sum map { $_->{rect}->{width} } @{$nodes};
#cmd 'open';
cmd 'resize grow left 10 px or 25 ppt';
cmd 'split v';
#cmd 'open';
cmd 'exec /usr/bin/urxvt';
sleep 0.5;
cmd 'mode toggle';
sleep 0.5;
cmd 'kill';

sleep 0.5;

($nodes, $focus) = get_ws_content($tmp);
my $new_sum = sum map { $_->{rect}->{width} } @{$nodes};

is($old_sum, $new_sum, 'combined container width is still equal');

done_testing;
