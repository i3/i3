#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
# Tests if the 'force_focus_wrapping' config directive works correctly.
#
use i3test;
use X11::XCB qw(:all);
use X11::XCB::Connection;

my $x = X11::XCB::Connection->new;

#####################################################################
# 1: test the wrapping behaviour without force_focus_wrapping
#####################################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $process = launch_with_config($config);

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $first = open_standard_window($x);
my $second = open_standard_window($x);

cmd 'layout tabbed';
cmd 'focus parent';

my $third = open_standard_window($x);
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

exit_gracefully($process->pid);

#####################################################################
# 2: test the wrapping behaviour with force_focus_wrapping
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
force_focus_wrapping true
EOT

$process = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$first = open_standard_window($x);
$second = open_standard_window($x);

cmd 'layout tabbed';
cmd 'focus parent';

$third = open_standard_window($x);

sync_with_i3($x);

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

exit_gracefully($process->pid);

done_testing;
