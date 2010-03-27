#!perl
# vim:ts=4:sw=4:expandtab

use Test::More tests => 3;
use Test::Exception;
use List::MoreUtils qw(all);
use FindBin;
use lib "$FindBin::Bin/lib";
use i3test;
use AnyEvent::I3;

my $i3 = i3;

####################
# Request workspaces
####################

my $workspaces = $i3->get_workspaces->recv;

ok(@{$workspaces} > 0, "More than zero workspaces found");

my $name_exists = all { defined($_->{name}) } @{$workspaces};
ok($name_exists, "All workspaces have a name");

diag( "Testing i3, Perl $], $^X" );
