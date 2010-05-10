#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests focus switching (next/prev)
#
use i3test tests => 13;
use X11::XCB qw(:all);
use v5.10;

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

######################################################################
# Open one container, verify that 'next v' and 'next h' do nothing
######################################################################
$i3->command('open')->recv;

my ($nodes, $focus) = get_ws_content($tmp);
my $old_focused = $focus->[0];

$i3->command('next v')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $old_focused, 'focus did not change with only one con');

$i3->command('next h')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $old_focused, 'focus did not change with only one con');

######################################################################
# Open another container, verify that 'next h' switches
######################################################################
my $left = $old_focused;

$i3->command('open')->recv;
($nodes, $focus) = get_ws_content($tmp);
isnt($old_focused, $focus->[0], 'new container is focused');
my $mid = $focus->[0];

$i3->command('open')->recv;
($nodes, $focus) = get_ws_content($tmp);
isnt($old_focused, $focus->[0], 'new container is focused');
my $right = $focus->[0];

$i3->command('next h')->recv;
($nodes, $focus) = get_ws_content($tmp);
isnt($focus->[0], $right, 'focus did change');
is($focus->[0], $left, 'left container focused (wrapping)');

$i3->command('next h')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $mid, 'middle container focused');

$i3->command('next h')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $right, 'right container focused');

$i3->command('prev h')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $mid, 'middle container focused');

$i3->command('prev h')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $left, 'left container focused');

$i3->command('prev h')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $right, 'right container focused');


######################################################################
# Test synonyms (horizontal/vertical instead of h/v)
######################################################################

$i3->command('prev horizontal')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $mid, 'middle container focused');

$i3->command('next horizontal')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $right, 'right container focused');

diag( "Testing i3, Perl $], $^X" );
