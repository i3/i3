#!perl
# vim:ts=4:sw=4:expandtab
#
use i3test i3_autostart => 0;
use X11::XCB qw(PROP_MODE_REPLACE);

##############################################################
# 1: test the following directive:
#    for_window [class="borderless"] border none
# by first creating a window with a different class (should get
# the normal border), then creating a window with the class
# "borderless" (should get no border)
##############################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [class="borderless"] border none
for_window [title="special borderless title"] border none
EOT

my $pid = launch_with_config($config);

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


# TODO: move this to X11::XCB::Window
sub set_wm_class {
    my ($id, $class, $instance) = @_;

    # Add a _NET_WM_STRUT_PARTIAL hint
    my $atomname = $x->atom(name => 'WM_CLASS');
    my $atomtype = $x->atom(name => 'STRING');

    $x->change_property(
        PROP_MODE_REPLACE,
        $id,
        $atomname->id,
        $atomtype->id,
        8,
        length($class) + length($instance) + 2,
        "$instance\x00$class\x00"
    );
}

$window = open_window(
    name => 'Borderless window',
    before_map => sub { set_wm_class($_->id, 'borderless', 'borderless') },
);

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'none', 'no border');

$window->unmap;
wait_for_unmap $window;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 0, 'no more nodes');

exit_gracefully($pid);

##############################################################
# 2: match on the title, check if for_window is really executed
# only once
##############################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [class="borderless"] border none
for_window [title="special borderless title"] border none
EOT

$pid = launch_with_config($config);

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

exit_gracefully($pid);

##############################################################
# 3: match on the title, set border style *and* a mark
##############################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [class="borderless" title="usethis"] border none
for_window [class="borderless"] border none
for_window [title="special borderless title"] border none
for_window [title="special mark title"] border none, mark bleh
EOT

$pid = launch_with_config($config);

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

exit_gracefully($pid);

##############################################################
# 4: multiple criteria for the for_window command
##############################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [class="borderless" title="usethis"] border none
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

$window = open_window(
    name => 'usethis',
    before_map => sub { set_wm_class($_->id, 'borderless', 'borderless') },
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

set_wm_class($window->id, 'borderless', 'borderless');
$window->name('notthis');
$window->map;
wait_for_map $window;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'normal', 'no border');


exit_gracefully($pid);

##############################################################
# 5: check that a class criterion does not match the instance
##############################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [class="foo"] border 1pixel
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;


$window = open_window(
    name => 'usethis',
    before_map => sub { set_wm_class($_->id, 'bar', 'foo') },
);

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'normal', 'normal border, not matched');

exit_gracefully($pid);

##############################################################
# 6: check that the 'instance' criterion works
##############################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [class="foo"] border 1pixel
for_window [instance="foo"] border none
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

$window = open_window(
    name => 'usethis',
    before_map => sub { set_wm_class($_->id, 'bar', 'foo') },
);

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'none', 'no border');

exit_gracefully($pid);

##############################################################
# 7: check that invalid criteria don’t end up matching all windows
##############################################################

# this configuration is broken because "asdf" is not a valid integer
# the for_window should therefore recognize this error and don’t add the
# assignment
$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [id="asdf"] border none
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

$window = open_window(
    name => 'usethis',
    before_map => sub { set_wm_class($_->id, 'bar', 'foo') },
);

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 1, 'one node on this workspace now');
is($content[0]->{border}, 'normal', 'normal border');

exit_gracefully($pid);

##############################################################
# 8: check that the role criterion works properly
##############################################################

# this configuration is broken because "asdf" is not a valid integer
# the for_window should therefore recognize this error and don’t add the
# assignment
$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [window_role="i3test"] border none
EOT

$pid = launch_with_config($config);

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

exit_gracefully($pid);

##############################################################
# 9: another test for the window_role, but this time it changes
#    *after* the window has been mapped
##############################################################

# this configuration is broken because "asdf" is not a valid integer
# the for_window should therefore recognize this error and don’t add the
# assignment
$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [window_role="i3test"] border none
EOT

$pid = launch_with_config($config);

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

exit_gracefully($pid);


done_testing;
