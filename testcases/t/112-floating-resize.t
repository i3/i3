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

fresh_workspace;

#####################################################################
# Create a floating window and see if resizing works
#####################################################################

my $window = open_floating_window;

# See if configurerequests cause window movements (they should not)
my ($a, $t) = $window->rect;
$window->rect(X11::XCB::Rect->new(x => $a->x, y => $a->y, width => $a->width, height => $a->height));

sync_with_i3;

my ($na, $nt) = $window->rect;
is_deeply($na, $a, 'Rects are equal after configurerequest');

sub test_resize {
    $window->rect(X11::XCB::Rect->new(x => 0, y => 0, width => 100, height => 100));

    sync_with_i3;

    my ($absolute, $top) = $window->rect;

    # Make sure the width/height are different from what we’re gonna test, so
    # that the test will work.
    isnt($absolute->width, 300, 'width != 300');
    isnt($absolute->height, 500, 'height != 500');

    $window->rect(X11::XCB::Rect->new(x => 0, y => 0, width => 300, height => 500));

    sync_with_i3;

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

################################################################################
# Check if we can position a floating window out of bounds. The Xephyr screen
# is 1280x1024, so x=2864, y=893 is out of bounds.
################################################################################

($a, $t) = $window->rect;
$window->rect(X11::XCB::Rect->new(
    x => 2864,
    y => 893,
    width => $a->width,
    height => $a->height));

sync_with_i3;

($a, $t) = $window->rect;
cmp_ok($a->x, '<', 1280, 'X not moved out of bounds');
cmp_ok($a->y, '<', 1024, 'Y not moved out of bounds');

done_testing;
