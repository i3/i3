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
#
# Test that `workspace {N}` acts like `workspace number {N}` when N is a plain
# digit, and likewise for `move to workspace {N}`.
# Ticket: #1238
# Bug still in: 4.8-16-g3f5a0f0
use i3test;

cmd 'workspace 5:foo';
open_window;
fresh_workspace;
cmd 'workspace 5';

is(focused_ws, '5:foo',
    'a command to switch to a workspace with a bare number should switch to a workspace of that number');

fresh_workspace;
my $win = open_window;
cmd '[id="' . $win->{id} . '"] move to workspace 5';

is(@{get_ws('5:foo')->{nodes}}, 2,
    'a command to move a container to a workspace with a bare number should move that container to a workspace of that number');

fresh_workspace;
cmd 'workspace 7';
open_window;
cmd 'workspace 7:foo';
$win = open_window;

cmd 'workspace 7';
is(focused_ws, '7',
    'a workspace with a name that is a matching plain number should be preferred when switching');

cmd '[id="' . $win->{id} . '"] move to workspace 7';
is(@{get_ws('7')->{nodes}}, 2,
    'a workspace with a name that is a matching plain number should be preferred when moving');

done_testing;
