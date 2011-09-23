#!perl
# vim:ts=4:sw=4:expandtab
#
# Test if the requested width/height is set after making the window floating.
#
use X11::XCB qw(:all);
use i3test;

my $tmp = fresh_workspace;

my $x = X11::XCB::Connection->new;

# Create a floating window which is smaller than the minimum enforced size of i3
my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 400, 150],
    background_color => '#C0C0C0',
);

isa_ok($window, 'X11::XCB::Window');

$window->map;

sleep 0.25;

my ($absolute, $top) = $window->rect;

ok($window->mapped, 'Window is mapped');
cmp_ok($absolute->{width}, '>', 400, 'i3 raised the width');
cmp_ok($absolute->{height}, '>', 150, 'i3 raised the height');

cmd 'floating toggle';
sync_with_i3($x);

($absolute, $top) = $window->rect;

diag('new width: ' . $absolute->{width});
diag('new height: ' . $absolute->{height});

# we compare with a tolerance of ± 20 pixels for borders in each direction
# (overkill, but hey)
cmp_ok($absolute->{width}, '>', 400-20, 'width now > 380');
cmp_ok($absolute->{width}, '<', 400+20, 'width now < 420');
cmp_ok($absolute->{height}, '>', 150-20, 'height now > 130');
cmp_ok($absolute->{height}, '<', 150+20, 'height now < 170');

#cmp_ok($absolute->{width}, '>=', 75, 'i3 raised the width to 75');
#cmp_ok($absolute->{height}, '>=', 50, 'i3 raised the height to 50');

done_testing;
