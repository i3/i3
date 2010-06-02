#!perl
# vim:ts=4:sw=4:expandtab

use i3test tests => 2;
use List::MoreUtils qw(all);

my $i3 = i3("/tmp/nestedcons");

####################
# Request workspaces
####################

SKIP: {
    skip "IPC API not yet stabilized", 2;

my $workspaces = $i3->get_workspaces->recv;

ok(@{$workspaces} > 0, "More than zero workspaces found");

my $name_exists = all { defined($_->{name}) } @{$workspaces};
ok($name_exists, "All workspaces have a name");

}

diag( "Testing i3, Perl $], $^X" );
