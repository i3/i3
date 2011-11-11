#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests if the 'border toggle' command works correctly
#
use i3test;

my $tmp = fresh_workspace;

cmd 'open';

my @nodes = @{get_ws_content($tmp)};
is(@nodes, 1, 'one container on this workspace');
is($nodes[0]->{border}, 'normal', 'border style normal');

cmd 'border 1pixel';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, '1pixel', 'border style 1pixel');

cmd 'border none';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'none', 'border style none');

cmd 'border normal';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'normal', 'border style back to normal');

cmd 'border toggle';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'none', 'border style none');

cmd 'border toggle';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, '1pixel', 'border style 1pixel');

cmd 'border toggle';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'normal', 'border style back to normal');

done_testing;
