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
# Verifies the 'focus output back_and_forth' command works properly.

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
# use 'focus output back_and_forth' and verify that focus changes as expected
################################################################################

sub focused_output {
    my $tree = $i3->get_tree->recv;
    my $focused = $tree->{focus}->[0];
    my $output = first { $_->{id} == $focused } @{$tree->{nodes}};
    return $output->{name};
}

sync_with_i3;
$x->root->warp_pointer(0, 0);
sync_with_i3;

is(focused_output, 'fake-0', 'focus on first output');
cmd 'workspace 1';
# ensure workspace 1 stays open
open_window;

cmd 'focus output back_and_forth';

# we have not changed focus because there is no previous output recorded.
# we should still be focused on the first workspace.
is(focused_output, 'fake-0', 'focus on first output');

cmd 'focus output right';
is(focused_output, 'fake-1', 'focus on the second output');

cmd 'focus output back_and_forth';
# we should have switched back to the first output at this point.
is(focused_output, 'fake-0', 'back to our previous output (fake-0)');

cmd 'focus output back_and_forth';
# now we should have switched back to the second output.
is(focused_output, 'fake-1', 'back to our previous output (fake-1)');

cmd 'workspace 2';
# ensure workspace 2 stays open
open_window;

# focus on workspace 1 on the first output
cmd 'workspace 1';
is(focused_ws, '1', 'workspace 1 on first output');

cmd 'focus output back_and_forth';

# we should now be focused on the second output
is(focused_output, 'fake-1', 'focus is on the second output');

exit_gracefully($pid);

done_testing;
