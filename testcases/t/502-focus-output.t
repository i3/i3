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
# Verifies the 'focus output' command works properly.

use i3test i3_autostart => 0;
use List::Util qw(first);

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT
my $pid = launch_with_config($config);

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

$x->root->warp_pointer(0, 0);
sync_with_i3;

is(focused_output, 'fake-0', 'focus on first output');

cmd 'focus output right';
is(focused_output, 'fake-1', 'focus on second output');

# focus should wrap when we focus to the right again.
cmd 'focus output right';
is(focused_output, 'fake-0', 'focus on first output again');

cmd 'focus output left';
is(focused_output, 'fake-1', 'focus back on second output');

cmd 'focus output left';
is(focused_output, 'fake-0', 'focus on first output again');

cmd 'focus output up';
is(focused_output, 'fake-0', 'focus still on first output');

cmd 'focus output down';
is(focused_output, 'fake-0', 'focus still on first output');

cmd 'focus output fake-1';
is(focused_output, 'fake-1', 'focus on second output');

cmd 'focus output fake-0';
is(focused_output, 'fake-0', 'focus on first output');

exit_gracefully($pid);

done_testing;
