#!perl
# vim:ts=4:sw=4:expandtab
#
# Check if stacking containers can be used independantly of
# the split mode (horizontal/vertical) of the underlying
# container.
#
use i3test tests => 7;
use Time::HiRes qw(sleep);

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

# Enforce vertical split mode
$i3->command('split v')->recv;

my $first = open_empty_con($i3);
my $second = open_empty_con($i3);

isnt($first, $second, 'two different containers opened');

##############################################################
# change mode to stacking and cycle through the containers
##############################################################

$i3->command('layout stacking')->recv;
my ($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $second, 'second container still focused');

$i3->command('next v')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $first, 'first container focused');

$i3->command('prev v')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $second, 'second container focused again');

##############################################################
# now change the orientation to horizontal and cycle
##############################################################

$i3->command('level up')->recv;
$i3->command('split h')->recv;
$i3->command('level down')->recv;

$i3->command('next v')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $first, 'first container focused');

$i3->command('prev v')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $second, 'second container focused again');


diag( "Testing i3, Perl $], $^X" );
