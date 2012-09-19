#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)

use i3test;

my $i3 = i3(get_socket_path());

####################
# Request workspaces
####################

SKIP: {
    skip "IPC API not yet stabilized", 2;

my $workspaces = $i3->get_workspaces->recv;

ok(@{$workspaces} > 0, "More than zero workspaces found");

#my $name_exists = all { defined($_->{name}) } @{$workspaces};
#ok($name_exists, "All workspaces have a name");

}

done_testing;
