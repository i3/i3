#!perl
# vim:ts=4:sw=4:expandtab

use Test::More tests => 10;
use Test::Deep;
use X11::XCB qw(:all);
use Data::Dumper;
use Time::HiRes qw(sleep);
use FindBin;
use lib "$FindBin::Bin/lib";
use i3test;

BEGIN {
    use_ok('X11::XCB::Window');
}

X11::XCB::Connection->connect(':0');

# Create a floating window which is smaller than the minimum enforced size of i3
my $window = X11::XCB::Window->new(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#C0C0C0',
    type => 'utility',
);

isa_ok($window, 'X11::XCB::Window');

$window->create;
$window->map;

sleep(0.25);

my ($absolute, $top) = $window->rect;

ok($window->mapped, 'Window is mapped');
ok($absolute->{width} >= 75, 'i3 raised the width to 75');
ok($absolute->{height} >= 50, 'i3 raised the height to 50');

ok($absolute->{x} != 0 && $absolute->{y} != 0, 'i3 did not map it to (0x0)');

$window->unmap;

$window = X11::XCB::Window->new(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 1, 1, 80, 90],
    background_color => '#C0C0C0',
    type => 'utility',
);

isa_ok($window, 'X11::XCB::Window');

$window->create;
$window->map;

sleep(0.25);

($absolute, $top) = $window->rect;

ok($absolute->{width} == 80, "i3 let the width at 80");
ok($absolute->{height} == 90, "i3 let the height at 90");

ok($top->{x} == 1 && $top->{y} == 1, "i3 mapped it to (1,1)");

$window->unmap;

diag( "Testing i3, Perl $], $^X" );
