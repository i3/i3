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
# Regression: moving a container which is the only child of the only child of a
# floating container crashes i3.
# Ticket: #3556
# Bug still in: 4.16-61-g376833db4
use i3test;

my $ws = fresh_workspace;
open_window;
open_window;
cmd 'split v, focus parent, floating toggle, focus child, move right';
does_i3_live;

$ws = get_ws($ws);
is(scalar @{$ws->{floating_nodes}}, 0, 'No floating nodes in workspace');
is(scalar @{$ws->{nodes}}, 2, 'Two tiling nodes in workspace');

done_testing;
