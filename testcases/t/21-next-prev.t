#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests focus switching (next/prev)
#
use i3test;

my $tmp = fresh_workspace;

######################################################################
# Open one container, verify that 'next v' and 'next h' do nothing
######################################################################
cmd 'open';

my ($nodes, $focus) = get_ws_content($tmp);
my $old_focused = $focus->[0];

cmd 'next v';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $old_focused, 'focus did not change with only one con');

cmd 'next h';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $old_focused, 'focus did not change with only one con');

######################################################################
# Open another container, verify that 'next h' switches
######################################################################
my $left = $old_focused;

cmd 'open';
($nodes, $focus) = get_ws_content($tmp);
isnt($old_focused, $focus->[0], 'new container is focused');
my $mid = $focus->[0];

cmd 'open';
($nodes, $focus) = get_ws_content($tmp);
isnt($old_focused, $focus->[0], 'new container is focused');
my $right = $focus->[0];

cmd 'next h';
($nodes, $focus) = get_ws_content($tmp);
isnt($focus->[0], $right, 'focus did change');
is($focus->[0], $left, 'left container focused (wrapping)');

cmd 'next h';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $mid, 'middle container focused');

cmd 'next h';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $right, 'right container focused');

cmd 'prev h';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $mid, 'middle container focused');

cmd 'prev h';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $left, 'left container focused');

cmd 'prev h';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $right, 'right container focused');


######################################################################
# Test synonyms (horizontal/vertical instead of h/v)
######################################################################

cmd 'prev horizontal';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $mid, 'middle container focused');

cmd 'next horizontal';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $right, 'right container focused');

######################################################################
# Test focus command
######################################################################

cmd qq|[con_id="$mid"] focus|;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $mid, 'middle container focused');


done_testing;
