#!perl
# vim:ts=4:sw=4:expandtab

use i3test;
use X11::XCB qw(:all);

BEGIN {
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

my $x = X11::XCB::Connection->new;

fresh_workspace;

#####################################################################
# Create a floating window and see if resizing works
#####################################################################

# Create a floating window
my $window = open_standard_window($x, undef, 1);

# See if configurerequests cause window movements (they should not)
my ($a, $t) = $window->rect;
$window->rect(X11::XCB::Rect->new(x => $a->x, y => $a->y, width => $a->width, height => $a->height));

sync_with_i3($x);

my ($na, $nt) = $window->rect;
is_deeply($na, $a, 'Rects are equal after configurerequest');

sub test_resize {
    $window->rect(X11::XCB::Rect->new(x => 0, y => 0, width => 100, height => 100));

    sync_with_i3($x);

    my ($absolute, $top) = $window->rect;

    # Make sure the width/height are different from what we’re gonna test, so
    # that the test will work.
    isnt($absolute->width, 300, 'width != 300');
    isnt($absolute->height, 500, 'height != 500');

    $window->rect(X11::XCB::Rect->new(x => 0, y => 0, width => 300, height => 500));

    sync_with_i3($x);

    ($absolute, $top) = $window->rect;

    is($absolute->width, 300, 'width = 300');
    is($absolute->height, 500, 'height = 500');
}

# Test with default border
test_resize;

# Test borderless
cmd 'border none';

test_resize;

# Test with 1-px-border
cmd 'border 1pixel';

test_resize;

done_testing;
