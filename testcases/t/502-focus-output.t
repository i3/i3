#!perl
# vim:ts=4:sw=4:expandtab
#
# Verifies the 'focus output' command works properly.

use i3test;
use List::Util qw(first);

my $tmp = fresh_workspace;
my $i3 = i3(get_socket_path());

################################################################################
# use 'focus output' and verify that focus gets changed appropriately
################################################################################

sub focused_output {
    my $tree = $i3->get_tree->recv;
    my $focused = $tree->{focus}->[0];
    my $output = first { $_->{id} == $focused } @{$tree->{nodes}};
    return $output->{name};
}

is(focused_output, 'xinerama-0', 'focus on first output');

cmd 'focus output right';
is(focused_output, 'xinerama-1', 'focus on second output');

# focus should wrap when we focus to the right again.
cmd 'focus output right';
is(focused_output, 'xinerama-0', 'focus on first output again');

cmd 'focus output left';
is(focused_output, 'xinerama-1', 'focus back on second output');

cmd 'focus output left';
is(focused_output, 'xinerama-0', 'focus on first output again');

cmd 'focus output up';
is(focused_output, 'xinerama-0', 'focus still on first output');

cmd 'focus output down';
is(focused_output, 'xinerama-0', 'focus still on first output');

cmd 'focus output xinerama-1';
is(focused_output, 'xinerama-1', 'focus on second output');

cmd 'focus output xinerama-0';
is(focused_output, 'xinerama-0', 'focus on first output');

done_testing;
