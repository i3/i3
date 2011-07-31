#!perl
# vim:ts=4:sw=4:expandtab
#
# Check if new containers are opened after the currently focused one instead
# of always at the end
use List::Util qw(first);
use i3test;

my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

# Open two new container
my $first = open_empty_con($i3);

ok(@{get_ws_content($tmp)} == 1, 'containers opened');

my $second = open_empty_con($i3);

isnt($first, $second, 'different container focused');

##############################################################
# see if new containers open after the currently focused
##############################################################

cmd qq|[con_id="$first"] focus|;
cmd 'open';
$content = get_ws_content($tmp);
ok(@{$content} == 3, 'three containers opened');

is($content->[0]->{id}, $first, 'first container unmodified');
isnt($content->[1]->{id}, $second, 'second container replaced');
is($content->[2]->{id}, $second, 'third container unmodified');

done_testing;
