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
# Test the 'focus workspace' command
use i3test i3_autostart => 0;

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

EOT

my $pid = launch_with_config($config);

sub switch_and_check {
    my ($cmd, $ws, $msg) = @_;
    cmd $cmd;
    is(focused_ws, $ws, "$msg: workspace '$ws' focused");
}


cmd 'workspace 1';
my $win1 = open_window(wm_class => 'win1');
switch_and_check('nop', '1', 'sanity check');

cmd 'workspace "other workspace"';
my $win2 = open_window(wm_class => 'win2');
switch_and_check('nop', 'other workspace', 'sanity check');

switch_and_check('[class=win1] focus workspace', '1', 'class criterion');
switch_and_check('[class=win2] focus workspace', 'other workspace', 'class criterion');
switch_and_check('[class=win2] focus workspace', 'other workspace', 'repeat class criterion');

switch_and_check("[id=$win1->{id}] focus workspace", '1', 'window id criterion');
switch_and_check("[id=$win1->{id}] focus workspace", '1', 'repeat window id criterion');
switch_and_check("[id=$win2->{id}] focus workspace", 'other workspace', 'window id criterion');

kill_all_windows;
exit_gracefully($pid);


#####################################################################
# Test Back and forth
# See #5744
#####################################################################
my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

workspace_auto_back_and_forth yes
EOT

my $pid = launch_with_config($config);

cmd 'workspace 1';
my $win1 = open_window(wm_class => 'win1');
switch_and_check('nop', '1', 'sanity check');

cmd 'workspace "other workspace"';
my $win2 = open_window(wm_class => 'win2');
switch_and_check('nop', 'other workspace', 'sanity check');

switch_and_check('[class=win1] focus workspace', '1', 'class criterion');
switch_and_check('[class=win2] focus workspace', 'other workspace', 'class criterion');
switch_and_check('[class=win2] focus workspace', '1', 'repeat class criterion');

cmd 'workspace 3';

switch_and_check("[id=$win1->{id}] focus workspace", '1', 'window id criterion');
switch_and_check("[id=$win1->{id}] focus workspace", '3', 'repeat window id criterion');
switch_and_check("[id=$win2->{id}] focus workspace", 'other workspace', 'window id criterion');
switch_and_check("[id=$win2->{id}] focus workspace", '3', 'window id criterion, back-and-forth');

exit_gracefully($pid);

done_testing;
