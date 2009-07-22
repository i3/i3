#!perl

use Test::More tests => 5;
use Test::Deep;
use X11::XCB qw(:all);

# We use relatively long sleeps (1/4 second) to make sure the window manager
# reacted.
use Time::HiRes qw(sleep);

BEGIN {
	use_ok('X11::XCB::Window');
}

X11::XCB::Connection->connect(':0');

my $original_rect = X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30);

my $window = X11::XCB::Window->new(
                class => WINDOW_CLASS_INPUT_OUTPUT,
		rect => $original_rect,
		#override_redirect => 1,
		background_color => 12632256
);

isa_ok($window, 'X11::XCB::Window');

is_deeply($window->rect, $original_rect, "rect unmodified before mapping");

$window->create;
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

diag( "Testing i3, Perl $], $^X" );
