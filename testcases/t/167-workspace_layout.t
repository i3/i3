#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
# Tests the workspace_layout config option.
#

use i3test;

#####################################################################
# 1: check that with an empty config, cons are place next to each
# other and no split containers are created
#####################################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $pid = launch_with_config($config);

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $first = open_window;
my $second = open_window;

sync_with_i3($x);

is($x->input_focus, $second->id, 'second window focused');
my @content = @{get_ws_content($tmp)};
ok(@content == 2, 'two containers opened');
isnt($content[0]->{layout}, 'stacked', 'layout not stacked');
isnt($content[1]->{layout}, 'stacked', 'layout not stacked');

exit_gracefully($pid);

#####################################################################
# 2: set workspace_layout stacked, check that when opening two cons,
# they end up in a stacked con
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
workspace_layout stacked
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$first = open_window;
$second = open_window;

sync_with_i3($x);

is($x->input_focus, $second->id, 'second window focused');
@content = @{get_ws_content($tmp)};
ok(@content == 1, 'one con at workspace level');
is($content[0]->{layout}, 'stacked', 'layout stacked');

#####################################################################
# 3: focus parent, open two new cons, check that they end up in a stacked
# con
#####################################################################

cmd 'focus parent';
my $right_top = open_window;
my $right_bot = open_window;

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

exit_gracefully($pid);

done_testing;
