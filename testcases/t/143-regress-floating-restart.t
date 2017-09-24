#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • https://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • https://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • https://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Regression: floating windows are tiling after restarting, closing them crashes i3
#
use i3test;

my $tmp = fresh_workspace;

cmd 'open';
cmd 'floating toggle';

my $ws = get_ws($tmp);
is(scalar @{$ws->{nodes}}, 0, 'no tiling nodes');
is(scalar @{$ws->{floating_nodes}}, 1, 'precisely one floating node');

cmd 'restart';

diag('Checking if i3 still lives');

does_i3_live;

$ws = get_ws($tmp);
is(scalar @{$ws->{nodes}}, 0, 'no tiling nodes');
is(scalar @{$ws->{floating_nodes}}, 1, 'precisely one floating node');

done_testing;
