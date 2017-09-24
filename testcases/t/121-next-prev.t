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
# Tests focus switching (next/prev)
#
use i3test;

my $tmp = fresh_workspace;

######################################################################
# Open one container, verify that 'focus down' and 'focus right' do nothing
######################################################################
cmd 'open';

my ($nodes, $focus) = get_ws_content($tmp);
my $old_focused = $focus->[0];

cmd 'focus down';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $old_focused, 'focus did not change with only one con');

cmd 'focus right';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $old_focused, 'focus did not change with only one con');

######################################################################
# Open another container, verify that 'focus right' switches
######################################################################
my $left = $old_focused;

cmd 'open';
($nodes, $focus) = get_ws_content($tmp);
isnt($old_focused, $focus->[0], 'new container is focused');
my $mid = $focus->[0];

cmd 'open';
($nodes, $focus) = get_ws_content($tmp);
isnt($old_focused, $focus->[0], 'new container is focused');
my $right = $focus->[0];

cmd 'focus right';
($nodes, $focus) = get_ws_content($tmp);
isnt($focus->[0], $right, 'focus did change');
is($focus->[0], $left, 'left container focused (wrapping)');

cmd 'focus right';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $mid, 'middle container focused');

cmd 'focus right';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $right, 'right container focused');

cmd 'focus left';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $mid, 'middle container focused');

cmd 'focus left';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $left, 'left container focused');

cmd 'focus left';
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $right, 'right container focused');


######################################################################
# Test focus command
######################################################################

cmd qq|[con_id="$mid"] focus|;
($nodes, $focus) = get_ws_content($tmp);
is($focus->[0], $mid, 'middle container focused');


done_testing;
