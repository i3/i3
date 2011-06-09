#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
# Tests the workspace_layout config option.
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
# 1: check that with an empty config, cons are place next to each
# other and no split containers are created
#####################################################################

my ($fh, $tmpfile) = tempfile();
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

is($x->input_focus, $second->id, 'second window focused');
ok(@{get_ws_content($tmp)} == 2, 'two containers opened');
isnt($content[0]->{layout}, 'stacked', 'layout not stacked');
isnt($content[1]->{layout}, 'stacked', 'layout not stacked');

exit_gracefully($process->pid);

#####################################################################
# 2: set workspace_layout stacked, check that when opening two cons,
# they end up in a stacked con
#####################################################################

($fh, $tmpfile) = tempfile();
say $fh "font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1";
say $fh "ipc-socket /tmp/nestedcons";
say $fh "workspace_layout stacked";
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

is($x->input_focus, $second->id, 'second window focused');
my @content = @{get_ws_content($tmp)};
ok(@content == 1, 'one con at workspace level');
is($content[0]->{layout}, 'stacked', 'layout stacked');

#####################################################################
# 3: focus parent, open two new cons, check that they end up in a stacked
# con
#####################################################################

cmd 'focus parent';
my $right_top = open_standard_window($x);
my $right_bot = open_standard_window($x);

@content = @{get_ws_content($tmp)};
is(@content, 2, 'two cons at workspace level after focus parent');
is($content[0]->{layout}, 'stacked', 'layout stacked');
is($content[1]->{layout}, 'stacked', 'layout stacked');

#####################################################################
# 4: move one of the cons to the right, check that it will end up in
# a stacked con
#####################################################################

cmd 'move right';

@content = @{get_ws_content($tmp)};
is(@content, 3, 'three cons at workspace level after move');
is($content[0]->{layout}, 'stacked', 'layout stacked');
is($content[1]->{layout}, 'stacked', 'layout stacked');
is($content[2]->{layout}, 'stacked', 'layout stacked');

#####################################################################
# 5: move it to the left again, check that the stacked con is deleted
#####################################################################

cmd 'move left';

@content = @{get_ws_content($tmp)};
is(@content, 2, 'two cons at workspace level after moving back');
is($content[0]->{layout}, 'stacked', 'layout stacked');
is($content[1]->{layout}, 'stacked', 'layout stacked');

#####################################################################
# 6: move it to a different workspace, check that it ends up in a
# stacked con
#####################################################################

my $otmp = get_unused_workspace;

cmd "move workspace $otmp";

@content = @{get_ws_content($tmp)};
is(@content, 2, 'still two cons on this workspace');
is($content[0]->{layout}, 'stacked', 'layout stacked');
is($content[1]->{layout}, 'stacked', 'layout stacked');

@content = @{get_ws_content($otmp)};
is(@content, 1, 'one con on target workspace');
is($content[0]->{layout}, 'stacked', 'layout stacked');

exit_gracefully($process->pid);

done_testing;
