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
# Tests if the 'force_focus_wrapping' config directive works correctly.
#
use i3test i3_autostart => 0;

#####################################################################
# 1: test the wrapping behaviour without force_focus_wrapping
#####################################################################

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $pid = launch_with_config($config);

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $first = open_window;
my $second = open_window;

cmd 'layout tabbed';
cmd 'focus parent';

my $third = open_window;
is($x->input_focus, $third->id, 'third window focused');

cmd 'focus left';
is($x->input_focus, $second->id, 'second window focused');

cmd 'focus left';
is($x->input_focus, $first->id, 'first window focused');

# now test the wrapping
cmd 'focus left';
is($x->input_focus, $second->id, 'second window focused');

# but focusing right should not wrap now, but instead focus the third window
cmd 'focus right';
is($x->input_focus, $third->id, 'third window focused');

exit_gracefully($pid);

#####################################################################
# 2: test the wrapping behaviour with force_focus_wrapping
#####################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
force_focus_wrapping true
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$first = open_window;
$second = open_window;

cmd 'layout tabbed';
cmd 'focus parent';

$third = open_window;

is($x->input_focus, $third->id, 'third window focused');

cmd 'focus left';
is($x->input_focus, $second->id, 'second window focused');

cmd 'focus left';
is($x->input_focus, $first->id, 'first window focused');

# now test the wrapping
cmd 'focus left';
is($x->input_focus, $second->id, 'second window focused');

# focusing right should now be forced to wrap
cmd 'focus right';
is($x->input_focus, $first->id, 'first window focused');

exit_gracefully($pid);

done_testing;
