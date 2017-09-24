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
# Tests if assignments work
#
use i3test i3_autostart => 0;

sub open_special {
    my %args = @_;
    $args{name} //= 'special window';

    # We use dont_map because i3 will not map the window on the current
    # workspace. Thus, open_window would time out in wait_for_map (2 seconds).
    my $window = open_window(
        %args,
        wm_class => 'special',
        dont_map => 1,
    );
    $window->map;
    return $window;
}

#####################################################################
# start a window and see that it does not get assigned with an empty config
#####################################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $pid = launch_with_config($config);

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $window = open_special;
wait_for_map($window);

ok(@{get_ws_content($tmp)} == 1, 'special window got managed to current (random) workspace');

exit_gracefully($pid);

$window->destroy;

#####################################################################
# start a window and see that it gets assigned to a formerly unused
# workspace
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
assign [class="special"] → targetws
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
my $workspaces = get_workspace_names;
ok(!("targetws" ~~ @{$workspaces}), 'targetws does not exist yet');

$window = open_special;
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'still no containers');
ok("targetws" ~~ @{get_workspace_names()}, 'targetws exists');

$window->destroy;

exit_gracefully($pid);

#####################################################################
# start a window and see that it gets assigned to a workspace which has content
# already, next to the existing node.
#####################################################################

$pid = launch_with_config($config);

# initialize the target workspace, then go to a fresh one
ok(!("targetws" ~~ @{get_workspace_names()}), 'targetws does not exist yet');
cmd 'workspace targetws';
cmp_ok(@{get_ws_content('targetws')}, '==', 0, 'no containers on targetws yet');
cmd 'open';
cmp_ok(@{get_ws_content('targetws')}, '==', 1, 'one container on targetws');
$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
ok("targetws" ~~ @{get_workspace_names()}, 'targetws does not exist yet');


# We use sync_with_i3 instead of wait_for_map here because i3 will not actually
# map the window -- it will be assigned to a different workspace and will only
# be mapped once you switch to that workspace
$window = open_special(dont_map => 1);
$window->map;
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'still no containers');
ok(@{get_ws_content('targetws')} == 2, 'two containers on targetws');

exit_gracefully($pid);

#####################################################################
# start a window and see that it gets assigned to a workspace which has content
# already, next to the existing node.
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [class="special"] floating enable
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
$workspaces = get_workspace_names;
ok(!("targetws" ~~ @{$workspaces}), 'targetws does not exist yet');

$window = open_special;
sync_with_i3;

my $content = get_ws($tmp);
ok(@{$content->{nodes}} == 0, 'no tiling cons');
ok(@{$content->{floating_nodes}} == 1, 'one floating con');

$window->destroy;

exit_gracefully($pid);

#####################################################################
# regression test: dock clients with floating assignments should not crash
# (instead, nothing should happen - dock clients can’t float)
# ticket #501
#####################################################################

# Walks /proc to figure out whether a child process of $i3pid with the name
# 'i3-nagbar' exists.
sub i3nagbar_running {
    my ($i3pid) = @_;

    my @procfiles = grep { m,^/proc/[0-9]+$, } </proc/*>;
    for my $path (@procfiles) {
        open(my $fh, '<', "$path/stat") or next;
        my $line = <$fh>;
        close($fh);
        my ($comm, $ppid) = ($line =~ /^[0-9]+ \(([^)]+)\) . ([0-9]+)/);
        return 1 if $ppid == $i3pid && $comm eq 'i3-nagbar';
    }
    return 0;
}

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [title="special"] floating enable
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
my @docked = get_dock_clients;
is(@docked, 0, 'one dock client yet');

$window = open_special(
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
);
sync_with_i3;

$content = get_ws($tmp);
ok(@{$content->{nodes}} == 0, 'no tiling cons');
ok(@{$content->{floating_nodes}} == 0, 'one floating con');
@docked = get_dock_clients;
is(@docked, 1, 'one dock client now');

$window->destroy;

does_i3_live;

exit_gracefully($pid);

done_testing;
