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
# Verifies that you can resize across different levels of containers even when
# they are all of the same orientation.
# (Ticket #754)
use i3test;

my $tmp = fresh_workspace;

open_window;
open_window;
cmd 'split v';
my $middle = open_window;
open_window;
cmd 'focus parent';
cmd 'split h';
open_window;

cmd '[id="' . $middle->id . '"] focus';
is($x->input_focus, $middle->id, 'middle window focused');

cmd 'resize grow left 10px or 25ppt';

my ($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.25, 'left container got only 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'right container got 75%');

done_testing;
