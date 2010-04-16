#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests whether opening an empty container and killing it again works
#
use Test::More tests => 3;
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

diag( "Testing i3, Perl $], $^X" );
