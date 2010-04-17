#!perl
# vim:ts=4:sw=4:expandtab

use Test::More tests => 19;
use Test::Deep;
use X11::XCB qw(:all);
use Data::Dumper;
use FindBin;
use lib "$FindBin::Bin/lib";
use i3test;
use AnyEvent::I3;
use List::Util qw(first);
use v5.10;

# We use relatively long sleeps (1/4 second) to make sure the window manager
# reacted.
use Time::HiRes qw(sleep);

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

# get the output of this workspace
my $tree = $i3->get_workspaces->recv;
my @outputs = @{$tree->{nodes}};
my $output = first { defined(first { $_->{name} eq $tmp } @{$_->{nodes}}) } @outputs;

BEGIN {
    use_ok('X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;

##################################
# map a window, then fullscreen it
##################################

my $original_rect = X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30);

my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => $original_rect,
    background_color => '#C0C0C0',
);

isa_ok($window, 'X11::XCB::Window');

is_deeply($window->rect, $original_rect, "rect unmodified before mapping");

$window->map;

sleep(0.25);

# open another container to make the window get only half of the screen
$i3->command('open')->recv;

my $new_rect = $window->rect;
ok(!eq_deeply($new_rect, $original_rect), "Window got repositioned");
$original_rect = $new_rect;

sleep(0.25);

$window->fullscreen(1);

sleep(0.25);

$new_rect = $window->rect;
ok(!eq_deeply($new_rect, $original_rect), "Window got repositioned after fullscreen");

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
$i3->command('open')->recv;

$original_rect = X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30);
$window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => $original_rect,
    background_color => 61440,
);

is_deeply($window->rect, $original_rect, "rect unmodified before mapping");

$window->fullscreen(1);
$window->map;

sleep(0.25);

$new_rect = $window->rect;
ok(!eq_deeply($new_rect, $original_rect), "Window got repositioned after fullscreen");
ok($window->mapped, "Window is mapped after opening it in fullscreen mode");

$wrect = $new_rect;

# see if the window really is fullscreen. 20 px for borders are allowed
ok(($wrect->{x} - $orect->{x}) < $threshold, 'x coordinate fullscreen');
ok(($wrect->{y} - $orect->{y}) < $threshold, 'y coordinate fullscreen');
ok(abs($wrect->{width} - $orect->{width}) < $threshold, 'width coordinate fullscreen');
ok(abs($wrect->{height} - $orect->{height}) < $threshold, 'height coordinate fullscreen');

###############################################################################
# test if setting two windows in fullscreen mode at the same time does not work
###############################################################################

$original_rect = X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30);
my $swindow = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => $original_rect,
    background_color => '#C0C0C0',
);

$swindow->map;
sleep(0.25);

ok(!$swindow->mapped, 'window not mapped while fullscreen window active');

$new_rect = $swindow->rect;
ok(!eq_deeply($new_rect, $original_rect), "Window got repositioned");

sleep(0.25);
$swindow->fullscreen(1);
sleep(0.25);

my $content = get_ws_content($tmp);

my $fullscreen_windows = grep { $_->{fullscreen_mode} != 0 } @{$content};
is($fullscreen_windows, 1, 'amount of fullscreen windows');

# clean up the workspace so that it will be cleaned when switching away
$i3->command('kill')->recv for (@{$content});

diag( "Testing i3, Perl $], $^X" );
