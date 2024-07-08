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
use i3test i3_autostart => 0;
use X11::XCB qw(PROP_MODE_REPLACE);

my (@nodes);

my $config = <<'EOT';
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

# test 1, test 2
for_window [class="borderless$"] border none
for_window [title="special borderless title"] border none

# test 3
for_window [class="borderless3$" title="usethis"] border none
for_window [class="borderless3$"] border none
for_window [title="special borderless title"] border none
for_window [title="special mark title"] border none, mark bleh

# test 4
for_window [class="borderless4$" title="usethis"] border none

# test 5, test 6
for_window [class="foo$"] border 1pixel

# test 6
for_window [instance="foo6"] border none

# test 7
for_window [id="asdf"] border none

# test 8, test 9
for_window [window_role="i3test"] border none

# test 12
for_window [workspace="trigger"] floating enable, mark triggered
EOT

# test all window types
my %window_types = (
    'normal'        => '_NET_WM_WINDOW_TYPE_NORMAL',
    'dialog'        => '_NET_WM_WINDOW_TYPE_DIALOG',
    'utility'       => '_NET_WM_WINDOW_TYPE_UTILITY',
    'toolbar'       => '_NET_WM_WINDOW_TYPE_TOOLBAR',
    'splash'        => '_NET_WM_WINDOW_TYPE_SPLASH',
    'menu'          => '_NET_WM_WINDOW_TYPE_MENU',
    'dropdown_menu' => '_NET_WM_WINDOW_TYPE_DROPDOWN_MENU',
    'popup_menu'    => '_NET_WM_WINDOW_TYPE_POPUP_MENU',
    'tooltip'       => '_NET_WM_WINDOW_TYPE_TOOLTIP',
    'notification'  => '_NET_WM_WINDOW_TYPE_NOTIFICATION'
);

for my $window_type (keys %window_types) {
    $config .= <<EOT;
for_window [window_type="$window_type"] floating enable, mark branded-$window_type
EOT
}

my $pid = launch_with_config($config);

##############################################################
# 1: test the following directive:
#    for_window [class="borderless"] border none
# by first creating a window with a different class (should get
# the normal border), then creating a window with the class
# "borderless" (should get no border)
##############################################################

my $tmp = fresh_workspace;

my $window = open_window(name => 'Border window');

my @content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'normal', 'normal border');

$window->unmap;
wait_for_unmap $window;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 0, 'no more nodes');
diag('content = '. Dumper(\@content));

$window = open_window(
    name => 'Borderless window',
    wm_class => 'borderless',
);

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'none', 'no border');

$window->unmap;
wait_for_unmap $window;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 0, 'no more nodes');

kill_all_windows;

##############################################################
# 2: match on the title, check if for_window is really executed
# only once
##############################################################

$tmp = fresh_workspace;

$window = open_window(name => 'special title');

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'normal', 'normal border');

$window->name('special borderless title');
sync_with_i3;

@content = @{get_ws_content($tmp)};
is($content[0]->{border}, 'none', 'no border');

$window->name('special title');
sync_with_i3;

cmd 'border normal';

@content = @{get_ws_content($tmp)};
is($content[0]->{border}, 'normal', 'border reset to normal');

$window->name('special borderless title');
sync_with_i3;

@content = @{get_ws_content($tmp)};
is($content[0]->{border}, 'normal', 'still normal border');

$window->unmap;
wait_for_unmap $window;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 0, 'no more nodes');

kill_all_windows;

##############################################################
# 3: match on the title, set border style *and* a mark
##############################################################

$tmp = fresh_workspace;

$window = open_window(name => 'special mark title');

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'none', 'no border');

my $other = open_window;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 2, 'two nodes');
is($content[0]->{border}, 'none', 'no border');
is($content[1]->{border}, 'normal', 'normal border');
ok(!$content[0]->{focused}, 'first one not focused');

cmd qq|[con_mark="bleh"] focus|;

@content = @{get_ws_content($tmp)};
ok($content[0]->{focused}, 'first node focused');

kill_all_windows;

##############################################################
# 4: multiple criteria for the for_window command
##############################################################

$tmp = fresh_workspace;

$window = open_window(
    name => 'usethis',
    wm_class => 'borderless4',
);

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'none', 'no border');

cmd 'kill';
wait_for_unmap $window;
$window->destroy;

# give i3 a chance to delete the window from its tree
sync_with_i3;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 0, 'no nodes on this workspace now');

$window->_create;
$window->wm_class('borderless4');
$window->name('notthis');
$window->map;
wait_for_map $window;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'normal', 'no border');

kill_all_windows;

##############################################################
# 5: check that a class criterion does not match the instance
##############################################################

$tmp = fresh_workspace;

$window = open_window(
    name => 'usethis',
    wm_class => 'bar',
    instance => 'foo',
);

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'normal', 'normal border, not matched');

kill_all_windows;

##############################################################
# 6: check that the 'instance' criterion works
##############################################################

$tmp = fresh_workspace;

$window = open_window(
    name => 'usethis',
    wm_class => 'bar',
    instance => 'foo6',
);

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'none', 'no border');

kill_all_windows;

##############################################################
# 7: check that invalid criteria don’t end up matching all windows
##############################################################

$tmp = fresh_workspace;

$window = open_window(
    name => 'usethis',
    wm_class => 'bar',
    instance => 'foo',
);

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'normal', 'normal border');

kill_all_windows;

##############################################################
# 8: check that the role criterion works properly
##############################################################

$tmp = fresh_workspace;

$window = open_window(
    name => 'usethis',
    before_map => sub {
        my ($window) = @_;
        my $atomname = $x->atom(name => 'WM_WINDOW_ROLE');
        my $atomtype = $x->atom(name => 'STRING');
        $x->change_property(
            PROP_MODE_REPLACE,
            $window->id,
            $atomname->id,
            $atomtype->id,
            8,
            length("i3test") + 1,
            "i3test\x00"
        );
    },
);

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'none', 'no border (window_role)');

kill_all_windows;

##############################################################
# 9: another test for the window_role, but this time it changes
#    *after* the window has been mapped
##############################################################

$tmp = fresh_workspace;

$window = open_window(name => 'usethis');

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'normal', 'normal border (window_role 2)');

my $atomname = $x->atom(name => 'WM_WINDOW_ROLE');
my $atomtype = $x->atom(name => 'STRING');
$x->change_property(
  PROP_MODE_REPLACE,
  $window->id,
  $atomname->id,
  $atomtype->id,
  8,
  length("i3test") + 1,
  "i3test\x00"
);

$x->flush;

sync_with_i3;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'none', 'no border (window_role 2)');

kill_all_windows;

##############################################################
# 10: check that the criterion 'window_type' works
##############################################################

while (my ($window_type, $atom) = each %window_types) {
    $tmp = fresh_workspace;

    $window = open_window(window_type => $x->atom(name => $atom));

    my @nodes = @{get_ws($tmp)->{floating_nodes}};
    cmp_ok(@nodes, '==', 1, 'one floating container on this workspace');
    is_deeply($nodes[0]->{nodes}[0]->{marks}, [ "branded-$window_type" ], "mark set (window_type = $atom)");

    kill_all_windows;
}

##############################################################
# 11: check that the criterion 'window_type' works if the
#     _NET_WM_WINDOW_TYPE is changed after managing.
##############################################################

while (my ($window_type, $atom) = each %window_types) {
    $tmp = fresh_workspace;

    $window = open_window();

    my $atomname = $x->atom(name => '_NET_WM_WINDOW_TYPE');
    my $atomtype = $x->atom(name => 'ATOM');
    $x->change_property(PROP_MODE_REPLACE, $window->id, $atomname->id, $atomtype->id,
      32, 1, pack('L1', $x->atom(name => $atom)->id));
    $x->flush;
    sync_with_i3;

    my @nodes = @{get_ws($tmp)->{floating_nodes}};
    cmp_ok(@nodes, '==', 1, 'one floating container on this workspace');
    is_deeply($nodes[0]->{nodes}[0]->{marks}, [ "branded-$window_type" ], "mark set (window_type = $atom)");

    kill_all_windows;
}

##############################################################
# 12: check that the criterion 'workspace' works
##############################################################

cmd 'workspace trigger';
$window = open_window;

@nodes = @{get_ws('trigger')->{floating_nodes}};
cmp_ok(@nodes, '==', 1, 'one floating container on this workspace');
is_deeply($nodes[0]->{nodes}[0]->{marks}, [ 'triggered' ], "mark set for workspace criterion");

kill_all_windows;

##############################################################

exit_gracefully($pid);

done_testing;
