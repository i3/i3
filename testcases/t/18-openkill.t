#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests whether opening an empty container and killing it again works
#
use Test::More tests => 6;
use Data::Dumper;
use FindBin;
use lib "$FindBin::Bin/lib";
use i3test;
use AnyEvent::I3;
use v5.10;

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

# Open a new container
$i3->command("open")->recv;

ok(@{get_ws_content($tmp)} == 1, 'container opened');

$i3->command("kill")->recv;
ok(@{get_ws_content($tmp)} == 0, 'container killed');

##############################################################
# open two containers and kill the one which is not focused
# by its ID to test if the parser correctly matches the window
##############################################################

$i3->command('open')->recv;
$i3->command('open')->recv;
ok(@{get_ws_content($tmp)} == 2, 'two containers opened');

my $content = get_ws_content($tmp);
# TODO: get the focused window, don’t assume that it is
# the latest one
my $id = $content->[0]->{id};
diag('id of not focused = ' . $id);

$i3->command("[con_id=\"$id\"] kill")->recv;

$content = get_ws_content($tmp);
ok(@{$content} == 1, 'one container killed');
ok($content->[0]->{id} != $id, 'correct window killed');

diag( "Testing i3, Perl $], $^X" );
