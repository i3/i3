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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Checks if the 'workspace back_and_forth' command and the
# 'workspace_auto_back_and_forth' config directive work correctly.
#

use i3test i3_autostart => 0;

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $pid = launch_with_config($config);

my $first_ws = fresh_workspace;
ok(get_ws($first_ws)->{focused}, 'first workspace focused');

my $second_ws = fresh_workspace;
ok(get_ws($second_ws)->{focused}, 'second workspace focused');

my $third_ws = fresh_workspace;
ok(get_ws($third_ws)->{focused}, 'third workspace focused');

cmd 'workspace back_and_forth';
ok(get_ws($second_ws)->{focused}, 'second workspace focused');

#####################################################################
# test that without workspace_auto_back_and_forth switching to the same
# workspace that is currently focused is a no-op
#####################################################################

cmd qq|workspace "$second_ws"|;
ok(get_ws($second_ws)->{focused}, 'second workspace still focused');

################################################################################
# verify that 'move workspace back_and_forth' works as expected
################################################################################

cmd qq|workspace "$first_ws"|;
my $first_win = open_window;

cmd qq|workspace "$second_ws"|;
my $second_win = open_window;

is(@{get_ws_content($first_ws)}, 1, 'one container on ws 1 before moving');
cmd 'move workspace back_and_forth';
is(@{get_ws_content($first_ws)}, 2, 'two containers on ws 1 before moving');

my $third_win = open_window;

################################################################################
# verify that moving to the current ws is a no-op without
# workspace_auto_back_and_forth.
################################################################################

cmd qq|workspace "$first_ws"|;

is(@{get_ws_content($second_ws)}, 1, 'one container on ws 2 before moving');
cmd qq|move workspace "$first_ws"|;
is(@{get_ws_content($second_ws)}, 1, 'still one container');

exit_gracefully($pid);

#####################################################################
# the same test, but with workspace_auto_back_and_forth
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
workspace_auto_back_and_forth yes
EOT

$pid = launch_with_config($config);

$first_ws = fresh_workspace;
ok(get_ws($first_ws)->{focused}, 'first workspace focused');

$second_ws = fresh_workspace;
ok(get_ws($second_ws)->{focused}, 'second workspace focused');

$third_ws = fresh_workspace;
ok(get_ws($third_ws)->{focused}, 'third workspace focused');

cmd qq|workspace "$third_ws"|;
ok(get_ws($second_ws)->{focused}, 'second workspace focused');
$first_win = open_window;

################################################################################
# verify that moving to the current ws moves to the previous one with
# workspace_auto_back_and_forth.
################################################################################

cmd qq|workspace "$first_ws"|;
$second_win = open_window;

is(@{get_ws_content($second_ws)}, 1, 'one container on ws 2 before moving');
cmd qq|move workspace "$first_ws"|;
is(@{get_ws_content($second_ws)}, 2, 'two containers on ws 2');

################################################################################
# Now see if "workspace number <number>" also works as expected with
# workspace_auto_back_and_forth enabled.
################################################################################

cmd 'workspace number 5';
ok(get_ws('5')->{focused}, 'workspace 5 focused');
# ensure it stays open
cmd 'open';

cmd 'workspace number 6';
ok(get_ws('6')->{focused}, 'workspace 6 focused');
# ensure it stays open
cmd 'open';

cmd 'workspace number 6';
is(focused_ws, '5', 'workspace 5 focused again');

################################################################################
# Rename the workspaces and see if workspace number still works with BAF.
################################################################################

cmd 'rename workspace 5 to 5: foo';
cmd 'rename workspace 6 to 6: baz';

is(focused_ws, '5: foo', 'workspace 5 still focused');

cmd 'workspace number 6';
is(focused_ws, '6: baz', 'workspace 6 now focused');

cmd 'workspace number 6';
is(focused_ws, '5: foo', 'workspace 5 focused again');

################################################################################
# Place a window in the scratchpad, see if BAF works after showing the
# scratchpad window.
################################################################################

my $scratchwin = open_window;
cmd 'move scratchpad';

# show scratchpad window
cmd 'scratchpad show';

# hide scratchpad window
cmd 'scratchpad show';

cmd 'workspace back_and_forth';
is(focused_ws, '6: baz', 'workspace 6 now focused');

################################################################################
# See if BAF is preserved after restart
################################################################################

cmd 'restart';
cmd 'workspace back_and_forth';
is(focused_ws, '5: foo', 'workspace 5 focused after restart');

################################################################################
# Check BAF switching to renamed workspace.
# Issue: #3694
################################################################################

kill_all_windows;
cmd 'workspace --no-auto-back-and-forth 1';
open_window;
cmd 'workspace --no-auto-back-and-forth 2';

cmd 'rename workspace 1 to 3';
cmd 'workspace back_and_forth';
is(focused_ws, '3', 'workspace 3 focused after rename');

################################################################################
# Check BAF switching to renamed and then closed workspace.
# Issue: #3694
################################################################################

kill_all_windows;
cmd 'workspace --no-auto-back-and-forth 1';
$first_win = open_window;
cmd 'workspace --no-auto-back-and-forth 2';

cmd 'rename workspace 1 to 3';
cmd '[id="' . $first_win->id . '"] kill';

cmd 'workspace back_and_forth';
is(focused_ws, '3', 'workspace 3 focused after renaming and destroying');

################################################################################
# See if renaming current workspace doesn't affect BAF switching to another
# renamed workspace.
# Issue: #3694
################################################################################

kill_all_windows;
cmd 'workspace --no-auto-back-and-forth 1';
$first_win = open_window;
cmd 'workspace --no-auto-back-and-forth 2';

cmd 'rename workspace 1 to 3';
cmd 'rename workspace 2 to 4';

cmd 'workspace back_and_forth';
is(focused_ws, '3', 'workspace 3 focused after renaming');

exit_gracefully($pid);

done_testing;
