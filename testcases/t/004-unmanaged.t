#!perl
# vim:ts=4:sw=4:expandtab

use i3test;

my $original_rect = X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30);

my $window = open_window(
    rect => $original_rect,
    override_redirect => 1,
    dont_map => 1,
);

isa_ok($window, 'X11::XCB::Window');

is_deeply($window->rect, $original_rect, "rect unmodified before mapping");

$window->map;

my $new_rect = $window->rect;
isa_ok($new_rect, 'X11::XCB::Rect');

is_deeply($new_rect, $original_rect, "window untouched");

done_testing;
