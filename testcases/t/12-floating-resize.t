#!perl
# vim:ts=4:sw=4:expandtab
# Beware that this test uses workspace 9 to perform some tests (it expects
# the workspace to be empty).
# TODO: skip it by default?

use i3test tests => 15;
use X11::XCB qw(:all);
use Time::HiRes qw(sleep);

BEGIN {
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

SKIP: {
    skip "border styles not yet implemented", 14;

my $x = X11::XCB::Connection->new;

my $i3 = i3("/tmp/nestedcons");
my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

#####################################################################
# Create a floating window and see if resizing works
#####################################################################

# Create a floating window
my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#C0C0C0',
    # replace the type with 'utility' as soon as the coercion works again in X11::XCB
    type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
);

isa_ok($window, 'X11::XCB::Window');

$window->map;
sleep 0.25;

# See if configurerequests cause window movements (they should not)
my ($a, $t) = $window->rect;
$window->rect(X11::XCB::Rect->new(x => $a->x, y => $a->y, width => $a->width, height => $a->height));

sleep 0.25;
my ($na, $nt) = $window->rect;
is_deeply($na, $a, 'Rects are equal after configurerequest');

sub test_resize {
    $window->rect(X11::XCB::Rect->new(x => 0, y => 0, width => 100, height => 100));

    my ($absolute, $top) = $window->rect;

    # Make sure the width/height are different from what we’re gonna test, so
    # that the test will work.
    isnt($absolute->width, 300, 'width != 300');
    isnt($absolute->height, 500, 'height != 500');

    $window->rect(X11::XCB::Rect->new(x => 0, y => 0, width => 300, height => 500));
    sleep 0.25;

    ($absolute, $top) = $window->rect;

    is($absolute->width, 300, 'width = 300');
    is($absolute->height, 500, 'height = 500');
}

# Test with default border
test_resize;

# Test borderless
$i3->command('bb')->recv;

test_resize;

# Test with 1-px-border
$i3->command('bp')->recv;

test_resize;

}
