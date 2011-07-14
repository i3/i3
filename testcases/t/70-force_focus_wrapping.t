#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
# Tests if the 'force_focus_wrapping' config directive works correctly.
#
use i3test;
use Cwd qw(abs_path);
use Proc::Background;
use File::Temp qw(tempfile tempdir);
use X11::XCB qw(:all);
use X11::XCB::Connection;

my $x = X11::XCB::Connection->new;

# assuming we are run by complete-run.pl
my $i3_path = abs_path("../i3");

#####################################################################
# 1: test the wrapping behaviour without force_focus_wrapping
#####################################################################

my ($fh, $tmpfile) = tempfile();
say $fh "# i3 config file (v4)";
say $fh "font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1";
say $fh "ipc-socket /tmp/nestedcons";
close($fh);

diag("Starting i3");
my $i3cmd = "exec " . abs_path("../i3") . " -V -d all --disable-signalhandler -c $tmpfile >/dev/null 2>/dev/null";
my $process = Proc::Background->new($i3cmd);
sleep 1;

diag("pid = " . $process->pid);

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

($fh, $tmpfile) = tempfile();
say $fh "# i3 config file (v4)";
say $fh "font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1";
say $fh "ipc-socket /tmp/nestedcons";
say $fh "force_focus_wrapping true";
close($fh);

diag("Starting i3");
$i3cmd = "exec " . abs_path("../i3") . " -V -d all --disable-signalhandler -c $tmpfile >/dev/null 2>/dev/null";
$process = Proc::Background->new($i3cmd);
sleep 1;

diag("pid = " . $process->pid);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$first = open_standard_window($x);
$second = open_standard_window($x);

cmd 'layout tabbed';
cmd 'focus parent';

$third = open_standard_window($x);
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
