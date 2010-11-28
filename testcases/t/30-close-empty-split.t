#!perl
# vim:ts=4:sw=4:expandtab
#
# Check if empty split containers are automatically closed.
#
use i3test tests => 4;
use Time::HiRes qw(sleep);

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $first = open_empty_con($i3);
my $second = open_empty_con($i3);
$i3->command(qq|[con_id="$first"] focus|)->recv;

$i3->command('split v')->recv;

($nodes, $focus) = get_ws_content($tmp);

is($nodes->[0]->{focused}, 0, 'split container not focused');

# focus the split container
$i3->command('level up')->recv;
($nodes, $focus) = get_ws_content($tmp);
my $split = $focus->[0];
$i3->command('level down')->recv;

my $second = open_empty_con($i3);

isnt($first, $second, 'different container focused');

##############################################################
# close both windows and see if the split container still exists
##############################################################

$i3->command('kill')->recv;
$i3->command('kill')->recv;
($nodes, $focus) = get_ws_content($tmp);
isnt($nodes->[0]->{id}, $split, 'split container closed');

diag( "Testing i3, Perl $], $^X" );
