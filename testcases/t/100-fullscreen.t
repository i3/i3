#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)

use i3test;
use List::Util qw(first);

my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

sub fullscreen_windows {
    my $ws = $tmp;
    $ws = shift if @_;

    scalar grep { $_->{fullscreen_mode} != 0 } @{get_ws_content($ws)}
}

# get the output of this workspace
my $tree = $i3->get_tree->recv;
my @outputs = @{$tree->{nodes}};
my $output;
for my $o (@outputs) {
    # get the first CT_CON of each output
    my $content = first { $_->{type} eq 'con' } @{$o->{nodes}};
    if (defined(first { $_->{name} eq $tmp } @{$content->{nodes}})) {
        $output = $o;
        last;
    }
}

##################################
# map a window, then fullscreen it
##################################

my $original_rect = X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30);

my $window = open_window(
    rect => $original_rect,
    dont_map => 1,
);

isa_ok($window, 'X11::XCB::Window');

is_deeply($window->rect, $original_rect, "rect unmodified before mapping");

$window->map;

wait_for_map $window;

# open another container to make the window get only half of the screen
cmd 'open';

my $new_rect = $window->rect;
ok(!eq_hash($new_rect, $original_rect), "Window got repositioned");
$original_rect = $new_rect;

$window->fullscreen(1);

sync_with_i3;

$new_rect = $window->rect;
ok(!eq_hash($new_rect, $original_rect), "Window got repositioned after fullscreen");

my $orect = $output->{rect};
my $wrect = $new_rect;

# see if the window really is fullscreen. 20 px for borders are allowed
my $threshold = 20;
ok(($wrect->{x} - $orect->{x}) < $threshold, 'x coordinate fullscreen');
ok(($wrect->{y} - $orect->{y}) < $threshold, 'y coordinate fullscreen');
ok(abs($wrect->{width} - $orect->{width}) < $threshold, 'width coordinate fullscreen');
ok(abs($wrect->{height} - $orect->{height}) < $threshold, 'height coordinate fullscreen');


$window->unmap;

#########################################################
# test with a window which is fullscreened before mapping
#########################################################

# open another container because the empty one will swallow the window we
# map in a second
cmd 'open';

$original_rect = X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30);
$window = open_window(
    rect => $original_rect,
    dont_map => 1,
);

is_deeply($window->rect, $original_rect, "rect unmodified before mapping");

$window->fullscreen(1);
$window->map;

wait_for_map $window;

$new_rect = $window->rect;
ok(!eq_hash($new_rect, $original_rect), "Window got repositioned after fullscreen");
ok($window->mapped, "Window is mapped after opening it in fullscreen mode");

$wrect = $new_rect;

# see if the window really is fullscreen. 20 px for borders are allowed
ok(($wrect->{x} - $orect->{x}) < $threshold, 'x coordinate fullscreen');
ok(($wrect->{y} - $orect->{y}) < $threshold, 'y coordinate fullscreen');
ok(abs($wrect->{width} - $orect->{width}) < $threshold, 'width coordinate fullscreen');
ok(abs($wrect->{height} - $orect->{height}) < $threshold, 'height coordinate fullscreen');

################################################################################
# Verify that when one window wants to go into fullscreen mode, the old
# fullscreen window will be replaced.
################################################################################

$original_rect = X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30);
my $swindow = open_window(
    rect => $original_rect,
    dont_map => 1,
);

$swindow->map;

sync_with_i3;

ok(!$swindow->mapped, 'window not mapped while fullscreen window active');

$new_rect = $swindow->rect;
ok(!eq_hash($new_rect, $original_rect), "Window got repositioned");

$swindow->fullscreen(1);
sync_with_i3;

is(fullscreen_windows(), 1, 'amount of fullscreen windows');

$window->fullscreen(0);
sync_with_i3;
is(fullscreen_windows(), 1, 'amount of fullscreen windows');

ok($swindow->mapped, 'window mapped after other fullscreen ended');

###########################################################################
# as $swindow is out of state at the moment (it requested to be fullscreen,
# but the WM denied), we check what happens if we go out of fullscreen now
# (nothing should happen)
###########################################################################

$swindow->fullscreen(0);
sync_with_i3;

is(fullscreen_windows(), 0, 'amount of fullscreen windows after disabling');

cmd 'fullscreen';

is(fullscreen_windows(), 1, 'amount of fullscreen windows after fullscreen command');

cmd 'fullscreen';

is(fullscreen_windows(), 0, 'amount of fullscreen windows after fullscreen command');

# clean up the workspace so that it will be cleaned when switching away
cmd 'kill' for (@{get_ws_content($tmp)});

################################################################################
# Verify that changing focus while in fullscreen does not work.
################################################################################

$tmp = fresh_workspace;

my $other = open_window;
is($x->input_focus, $other->id, 'other window focused');

$window = open_window;
is($x->input_focus, $window->id, 'window focused');

cmd 'fullscreen';
is($x->input_focus, $window->id, 'fullscreen window focused');

cmd 'focus left';
is($x->input_focus, $window->id, 'fullscreen window still focused');

################################################################################
# Verify that fullscreening a window on a second workspace and moving it onto
# the first workspace unfullscreens the first window.
################################################################################

my $tmp2 = fresh_workspace;
$swindow = open_window;

cmd 'fullscreen';

is(fullscreen_windows($tmp2), 1, 'one fullscreen window on second ws');

cmd "move workspace $tmp";

is(fullscreen_windows($tmp2), 0, 'no fullscreen windows on second ws');
is(fullscreen_windows($tmp), 1, 'one fullscreen window on first ws');

$swindow->fullscreen(0);
sync_with_i3;

# Verify that $swindow was the one that initially remained fullscreen.
is(fullscreen_windows($tmp), 0, 'no fullscreen windows on first ws');

################################################################################
# Verify that opening a window with _NET_WM_STATE_FULLSCREEN unfullscreens any
# existing container on the workspace and fullscreens the newly opened window.
################################################################################

$tmp = fresh_workspace;

$window = open_window();

cmd "fullscreen";

is(fullscreen_windows($tmp), 1, 'one fullscreen window on ws');
is($x->input_focus, $window->id, 'fullscreen window focused');

$swindow = open_window({
    fullscreen => 1
});

is(fullscreen_windows($tmp), 1, 'one fullscreen window on ws');
is($x->input_focus, $swindow->id, 'fullscreen window focused');

done_testing;
