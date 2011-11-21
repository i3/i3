#!perl
# vim:ts=4:sw=4:expandtab
#
# Check if empty split containers are automatically closed.
#
use i3test;

my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $first = open_empty_con($i3);
my $second = open_empty_con($i3);
cmd qq|[con_id="$first"] focus|;

cmd 'split v';

my ($nodes, $focus) = get_ws_content($tmp);

is($nodes->[0]->{focused}, 0, 'split container not focused');

# focus the split container
cmd 'level up';
($nodes, $focus) = get_ws_content($tmp);
my $split = $focus->[0];
cmd 'level down';

$second = open_empty_con($i3);

isnt($first, $second, 'different container focused');

##############################################################
# close both windows and see if the split container still exists
##############################################################

cmd 'kill';
cmd 'kill';
($nodes, $focus) = get_ws_content($tmp);
isnt($nodes->[0]->{id}, $split, 'split container closed');

##############################################################
# same thing but this time we are moving the cons away instead
# of killing them
##############################################################

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$first = open_empty_con($i3);
$second = open_empty_con($i3);
cmd qq|[con_id="$first"] focus|;

cmd 'split v';

($nodes, $focus) = get_ws_content($tmp);

is($nodes->[0]->{focused}, 0, 'split container not focused');

# focus the split container
cmd 'level up';
($nodes, $focus) = get_ws_content($tmp);
$split = $focus->[0];
cmd 'level down';

$second = open_empty_con($i3);

isnt($first, $second, 'different container focused');

##############################################################
# close both windows and see if the split container still exists
##############################################################

my $otmp = get_unused_workspace();
cmd "move workspace $otmp";
cmd "move workspace $otmp";
($nodes, $focus) = get_ws_content($tmp);
isnt($nodes->[0]->{id}, $split, 'split container closed');

done_testing;
