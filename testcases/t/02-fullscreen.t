#!perl

use Test::More tests => 8;
use Test::Deep;
use X11::XCB qw(:all);
use Data::Dumper;

# We use relatively long sleeps (1/4 second) to make sure the window manager
# reacted.
use Time::HiRes qw(sleep);

BEGIN {
	use_ok('X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;

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

my $new_rect = $window->rect;
ok(!eq_deeply($new_rect, $original_rect), "Window got repositioned");
$original_rect = $new_rect;

sleep(0.25);

$window->fullscreen(1);

sleep(0.25);

$new_rect = $window->rect;
ok(!eq_deeply($new_rect, $original_rect), "Window got repositioned after fullscreen");

$window->unmap;

$window = $x->root->create_child(
	class => WINDOW_CLASS_INPUT_OUTPUT,
	rect => $original_rect,
	background_color => 61440,
);

is_deeply($window->rect, $original_rect, "rect unmodified before mapping");

$window->fullscreen(1);
$window->map;

sleep(0.25);

ok(!eq_deeply($new_rect, $original_rect), "Window got repositioned after fullscreen");
ok($window->mapped, "Window is mapped after opening it in fullscreen mode");

diag( "Testing i3, Perl $], $^X" );
