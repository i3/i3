#!perl
# vim:ts=4:sw=4:expandtab
#
# Check if new containers are opened after the currently focused one instead
# of always at the end
use List::Util qw(first);
use i3test tests => 7;

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

# Open two new container
$i3->command("open")->recv;

ok(@{get_ws_content($tmp)} == 1, 'containers opened');

my ($nodes, $focus) = get_ws_content($tmp);
my $first = $focus->[0];

$i3->command("open")->recv;

($nodes, $focus) = get_ws_content($tmp);
my $second = $focus->[0];

isnt($first, $second, 'different container focused');

##############################################################
# see if new containers open after the currently focused
##############################################################

$i3->command(qq|[con_id="$first"] focus|)->recv;
$i3->command('open')->recv;
$content = get_ws_content($tmp);
ok(@{$content} == 3, 'three containers opened');

is($content->[0]->{id}, $first, 'first container unmodified');
isnt($content->[1]->{id}, $second, 'second container replaced');
is($content->[2]->{id}, $second, 'third container unmodified');

diag( "Testing i3, Perl $], $^X" );
#

