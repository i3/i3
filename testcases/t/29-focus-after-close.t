#!perl
# vim:ts=4:sw=4:expandtab
#
# Check if the focus is correctly restored after closing windows.
#
use i3test tests => 6;
use Time::HiRes qw(sleep);

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$i3->command('open')->recv;
my ($nodes, $focus) = get_ws_content($tmp);
my $first = $focus->[0];

$i3->command('split v')->recv;

($nodes, $focus) = get_ws_content($tmp);

is($nodes->[0]->{focused}, 0, 'split container not focused');
$i3->command('level up')->recv;
($nodes, $focus) = get_ws_content($tmp);
is($nodes->[0]->{focused}, 1, 'split container focused after level up');

$i3->command('open')->recv;

($nodes, $focus) = get_ws_content($tmp);
my $second = $focus->[0];

isnt($first, $second, 'different container focused');

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
is($nodes->[0]->{nodes}->[0]->{id}, $first, 'first container found');
is($nodes->[0]->{nodes}->[0]->{focused}, 1, 'first container focused');

diag( "Testing i3, Perl $], $^X" );
