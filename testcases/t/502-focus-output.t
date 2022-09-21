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
# Verifies the 'focus output' command works properly.

use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT
use List::Util qw(first);

my $tmp = fresh_workspace;
my $i3 = i3(get_socket_path());

################################################################################
# use 'focus output' and verify that focus gets changed appropriately
################################################################################

sync_with_i3;
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

################################################################################
# use 'focus output' and verify that i3 does not crash when the currently
# focused window is floating and is only partially mapped on an output screen
################################################################################

is(focused_output, 'fake-0', 'focus on first output');

my $floating_win = open_window;
cmd 'floating toggle';
cmd 'move to absolute position -10 -10';

cmd 'focus output right';
is(focused_output, 'fake-1', 'focus on second output');

cmd 'focus output fake-0';
is(focused_output, 'fake-0', 'focus on first output');

################################################################################
# use 'focus output' with command criteria and verify that i3 does not crash
# when they don't match any window
################################################################################

is(focused_output, 'fake-0', 'focus on first output');

cmd '[con_mark=doesnotexist] focus output right';
does_i3_live;
is(focused_output, 'fake-0', 'focus remained on first output');

################################################################################
# use 'focus output' with command criteria and verify that focus gets changed
# appropriately
################################################################################

is(focused_output, 'fake-0', 'focus on first output');

my $window = open_window;

cmd 'focus output right';
is(focused_output, 'fake-1', 'focus on second output');

cmd '[id= . ' . $window->id . '] focus output right';
is(focused_output, 'fake-1', 'focus on second output after command with criteria');

cmd 'focus output right';
is(focused_output, 'fake-0', 'focus on first output after command without criteria');

done_testing;
