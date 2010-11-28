#!perl
# vim:ts=4:sw=4:expandtab
#
# Check if the focus is correctly restored after closing windows.
#
use i3test tests => 9;
use List::Util qw(first);
use Time::HiRes qw(sleep);

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $first = open_empty_con($i3);
my $second = open_empty_con($i3);

$i3->command('split v')->recv;

my ($nodes, $focus) = get_ws_content($tmp);

is($nodes->[1]->{focused}, 0, 'split container not focused');
$i3->command('level up')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($nodes->[1]->{focused}, 1, 'split container focused after level up');

my $third = open_empty_con($i3);

isnt(get_focused($tmp), $second, 'different container focused');

# We have the following layout now (con is focused):
# .----------------.
# | split  |       |
# | .----. |  con  |
# | | cn | |       |
# | `----' |       |
# `----------------'

##############################################################
# see if the focus goes down to $first (not to its split parent)
# when closing $second
##############################################################

$i3->command('kill')->recv;
# TODO: this testcase sometimes has different outcomes when the
# sleep is missing. why?
sleep 0.25;
($nodes, $focus) = get_ws_content($tmp);
is($nodes->[1]->{nodes}->[0]->{id}, $second, 'second container found');
is($nodes->[1]->{nodes}->[0]->{focused}, 1, 'second container focused');

##############################################################
# another case, using a slightly different layout (regression)
##############################################################

$tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$i3->command('split v')->recv;
$first = open_empty_con($i3);
my $bottom = open_empty_con($i3);

$i3->command('prev v')->recv;
$i3->command('split h')->recv;
my $middle = open_empty_con($i3);
my $right = open_empty_con($i3);
$i3->command('next v')->recv;

# We have the following layout now (second is focused):
# .----------------------------.
# | .------------------------. |
# | | first | middle | right | |
# | `------------------------' |
# |----------------------------|
# |                            |
# |          second            |
# |                            |
# `----------------------------'

# Check if the focus is restored to $right when we close $second
$i3->command('kill')->recv;

is(get_focused($tmp), $right, 'top right container focused (in focus stack)');

($nodes, $focus) = get_ws_content($tmp);
my $tr = first { $_->{id} eq $right } @{$nodes->[0]->{nodes}};
is($tr->{focused}, 1, 'top right container really has focus');

##############################################################
# and now for something completely different:
# check if the pointer position is relevant when restoring focus
# (it should not be relevant, of course)
##############################################################

# TODO: add test code as soon as I can reproduce it

diag( "Testing i3, Perl $], $^X" );
