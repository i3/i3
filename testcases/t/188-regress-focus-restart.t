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
# Verifies that i3 survives inplace restarts with fullscreen containers
#
use i3test;

my $tmp = fresh_workspace;

open_window(name => 'first');
open_window(name => 'second');

cmd 'focus left';

my ($nodes, $focus) = get_ws_content($tmp);
is(scalar @$nodes, 2, 'two tiling nodes on workspace');
is($nodes->[0]->{name}, 'first', 'first node name ok');
is($nodes->[1]->{name}, 'second', 'second node name ok');
is($focus->[0], $nodes->[0]->{id}, 'first node focused');
is($focus->[1], $nodes->[1]->{id}, 'second node second in focus stack');

cmd 'restart';

does_i3_live;

($nodes, $focus) = get_ws_content($tmp);
is(scalar @$nodes, 2, 'still two tiling nodes on workspace');
is($nodes->[0]->{name}, 'first', 'first node name ok');
is($nodes->[1]->{name}, 'second', 'second node name ok');
is($focus->[0], $nodes->[0]->{id}, 'first node focused');
is($focus->[1], $nodes->[1]->{id}, 'second node second in focus stack');

done_testing;
