#!perl
# vim:ts=4:sw=4:expandtab

use Test::More tests => 5;
use Test::Deep;
use X11::XCB qw(:all);
use Data::Dumper;

BEGIN {
    use_ok('X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;

my $original_rect = X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30);

my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => $original_rect,
    override_redirect => 1,
    background_color => '#C0C0C0',
);

isa_ok($window, 'X11::XCB::Window');

is_deeply($window->rect, $original_rect, "rect unmodified before mapping");

$window->map;

my $new_rect = $window->rect;
isa_ok($new_rect, 'X11::XCB::Rect');

is_deeply($new_rect, $original_rect, "window untouched");

diag( "Testing i3, Perl $], $^X" );
