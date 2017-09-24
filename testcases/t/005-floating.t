#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • https://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • https://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • https://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)

use i3test;

# Create a floating window which is smaller than the minimum enforced size of i3
my $window = open_floating_window;

isa_ok($window, 'X11::XCB::Window');

my ($absolute, $top) = $window->rect;

ok($window->mapped, 'Window is mapped');
cmp_ok($absolute->{width}, '>=', 75, 'i3 raised the width to 75');
cmp_ok($absolute->{height}, '>=', 50, 'i3 raised the height to 50');

ok($absolute->{x} != 0 && $absolute->{y} != 0, 'i3 did not map it to (0x0)');

$window->unmap;

$window = open_floating_window(rect => [ 20, 20, 80, 90 ]);

isa_ok($window, 'X11::XCB::Window');

($absolute, $top) = $window->rect;

cmp_ok($absolute->{width}, '==', 80, "i3 let the width at 80");
cmp_ok($absolute->{height}, '==', 90, "i3 let the height at 90");

cmp_ok($top->{x}, '==', 20, 'i3 mapped it to x=20');
cmp_ok($top->{y}, '==', 20, 'i3 mapped it to y=20');

$window->unmap;

#####################################################################
# check that a tiling window which is then made floating still has
# at least the size of its initial geometry
#####################################################################

$window = open_window(rect => [ 1, 1, 80, 90 ]);

isa_ok($window, 'X11::XCB::Window');

cmd 'floating enable';
sync_with_i3;

($absolute, $top) = $window->rect;

cmp_ok($absolute->{width}, '==', 80, "i3 let the width at 80");
cmp_ok($absolute->{height}, '==', 90, "i3 let the height at 90");

$window->unmap;

done_testing;
