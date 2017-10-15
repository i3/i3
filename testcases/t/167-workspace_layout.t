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
# Tests the workspace_layout config option.
#

use i3test i3_autostart => 0;

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

#####################################################################
# 7: toggle floating mode and check that we have a stacked con when
# re-inserting a floating container.
#####################################################################

$tmp = fresh_workspace;

$first = open_window;
cmd 'floating toggle';
cmd 'floating toggle';

$second = open_window;

is($x->input_focus, $second->id, 'second window focused');
@content = @{get_ws_content($tmp)};
ok(@content == 1, 'one con at workspace level');
is($content[0]->{layout}, 'stacked', 'layout stacked');

#####################################################################
# 8: when the workspace is empty check that its layout can be changed
# from stacked to horizontal split using the 'layout splith' command.
#####################################################################

$tmp = fresh_workspace;

cmd 'layout stacked';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
is($content[0]->{layout}, 'stacked', 'layout stacked');

cmd '[id="' . $first->id . '"] kill';
cmd '[id="' . $second->id . '"] kill';
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'workspace is empty');

cmd 'layout splith';
$first = open_window;
$second = open_window;
@content = @{get_ws_content($tmp)};
ok(@content == 2, 'two containers opened');
isnt($content[0]->{layout}, 'stacked', 'layout not stacked');
isnt($content[1]->{layout}, 'stacked', 'layout not stacked');

#####################################################################
# 9: when the workspace is empty check that its layout can be changed
# from stacked to vertical split using the 'layout splitv' command.
#####################################################################

$tmp = fresh_workspace;

cmd 'layout stacked';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
is($content[0]->{layout}, 'stacked', 'layout stacked');

cmd '[id="' . $first->id . '"] kill';
cmd '[id="' . $second->id . '"] kill';
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'workspace is empty');

cmd 'layout splitv';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
ok(@content == 2, 'two containers opened');
isnt($content[0]->{layout}, 'stacked', 'layout not stacked');
isnt($content[1]->{layout}, 'stacked', 'layout not stacked');

#####################################################################
# 10: when the workspace is empty check that its layout can be changed
# from tabbed to horizontal split using the 'layout splith' command.
#####################################################################

$tmp = fresh_workspace;

cmd 'layout tabbed';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
is($content[0]->{layout}, 'tabbed', 'layout tabbed');

cmd '[id="' . $first->id . '"] kill';
cmd '[id="' . $second->id . '"] kill';
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'workspace is empty');

cmd 'layout splith';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
ok(@content == 2, 'two containers opened');
isnt($content[0]->{layout}, 'tabbed', 'layout not tabbed');
isnt($content[1]->{layout}, 'tabbed', 'layout not tabbed');

#####################################################################
# 11: when the workspace is empty check that its layout can be changed
# from tabbed to vertical split using the 'layout splitv' command.
#####################################################################

$tmp = fresh_workspace;

cmd 'layout tabbed';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
is($content[0]->{layout}, 'tabbed', 'layout tabbed');

cmd '[id="' . $first->id . '"] kill';
cmd '[id="' . $second->id . '"] kill';
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'workspace is empty');

cmd 'layout splitv';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
ok(@content == 2, 'two containers opened');
isnt($content[0]->{layout}, 'tabbed', 'layout not tabbed');
isnt($content[1]->{layout}, 'tabbed', 'layout not tabbed');

#####################################################################
# 12: when the workspace is empty check that its layout can be changed
# from stacked to horizontal split using the 'split horizontal' command.
#####################################################################

$tmp = fresh_workspace;

cmd 'layout stacked';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
is($content[0]->{layout}, 'stacked', 'layout stacked');

cmd '[id="' . $first->id . '"] kill';
cmd '[id="' . $second->id . '"] kill';
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'workspace is empty');

cmd 'split horizontal';
$first = open_window;
$second = open_window;
@content = @{get_ws_content($tmp)};
ok(@content == 2, 'two containers opened');
isnt($content[0]->{layout}, 'stacked', 'layout not stacked');
isnt($content[1]->{layout}, 'stacked', 'layout not stacked');

#####################################################################
# 13: when the workspace is empty check that its layout can be changed
# from stacked to vertical split using the 'split vertical' command.
#####################################################################

$tmp = fresh_workspace;

cmd 'layout stacked';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
is($content[0]->{layout}, 'stacked', 'layout stacked');

cmd '[id="' . $first->id . '"] kill';
cmd '[id="' . $second->id . '"] kill';
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'workspace is empty');

cmd 'split vertical';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
ok(@content == 2, 'two containers opened');
isnt($content[0]->{layout}, 'stacked', 'layout not stacked');
isnt($content[1]->{layout}, 'stacked', 'layout not stacked');

#####################################################################
# 14: when the workspace is empty check that its layout can be changed
# from tabbed to horizontal split using the 'split horizontal' command.
#####################################################################

$tmp = fresh_workspace;

cmd 'layout tabbed';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
is($content[0]->{layout}, 'tabbed', 'layout tabbed');

cmd '[id="' . $first->id . '"] kill';
cmd '[id="' . $second->id . '"] kill';
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'workspace is empty');

cmd 'split horizontal';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
ok(@content == 2, 'two containers opened');
isnt($content[0]->{layout}, 'tabbed', 'layout not tabbed');
isnt($content[1]->{layout}, 'tabbed', 'layout not tabbed');

#####################################################################
# 15: when the workspace is empty check that its layout can be changed
# from tabbed to vertical split using the 'split vertical' command.
#####################################################################

$tmp = fresh_workspace;

cmd 'layout tabbed';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
is($content[0]->{layout}, 'tabbed', 'layout tabbed');

cmd '[id="' . $first->id . '"] kill';
cmd '[id="' . $second->id . '"] kill';
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'workspace is empty');

cmd 'split vertical';
$first = open_window;
$second = open_window;

@content = @{get_ws_content($tmp)};
ok(@content == 2, 'two containers opened');
isnt($content[0]->{layout}, 'tabbed', 'layout not tabbed');
isnt($content[1]->{layout}, 'tabbed', 'layout not tabbed');

exit_gracefully($pid);

#####################################################################
# 16: Check that the command 'layout toggle split' works regardless
# of what layout we're using.
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
workspace_layout default
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

my @layouts = ('splith', 'splitv', 'tabbed', 'stacked');
my $first_layout;

foreach $first_layout (@layouts) {
    cmd 'layout ' . $first_layout;
    $first = open_window;
    $second = open_window;
    cmd 'layout toggle split';
    @content = @{get_ws_content($tmp)};
    if ($first_layout eq 'splith') {
        is($content[0]->{layout}, 'splitv', 'layout toggles to splitv');
    } else {
        is($content[0]->{layout}, 'splith', 'layout toggles to splith');
    }

    cmd '[id="' . $first->id . '"] kill';
    cmd '[id="' . $second->id . '"] kill';
    sync_with_i3;
}

exit_gracefully($pid);

#####################################################################
# 17: Check about setting a new layout.
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
workspace_layout default
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

my $second_layout;

foreach $first_layout (@layouts) {
    foreach $second_layout (@layouts) {
        cmd 'layout ' . $first_layout;
        $first = open_window;
        $second = open_window;
        cmd 'layout ' . $second_layout;
        @content = @{get_ws_content($tmp)};
        is($content[0]->{layout}, $second_layout, 'layout changes to ' . $second_layout);

        cmd '[id="' . $first->id . '"] kill';
        cmd '[id="' . $second->id . '"] kill';
        sync_with_i3;
    }
}

done_testing;
