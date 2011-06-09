#!perl
# vim:ts=4:sw=4:expandtab
#
# Test to see if i3 combines the geometry of all children in a split container
# when setting the split container to floating
#
use X11::XCB qw(:all);
use i3test;

BEGIN {
    use_ok('X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;
my $i3 = i3("/tmp/nestedcons");

my $tmp = fresh_workspace;

#####################################################################
# open a window with 200x80
#####################################################################

my $first = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 200, 80],
    background_color => '#FF0000',
);

$first->map;

sleep 0.25;

#####################################################################
# Open a second window with 300x90
#####################################################################

my $second = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 300, 90],
    background_color => '#00FF00',
);

$second->map;

sleep 0.25;

#####################################################################
# Set the parent to floating
#####################################################################
cmd 'nop setting floating';
cmd 'focus parent';
cmd 'mode floating';

#####################################################################
# Get geometry of the first floating node (the split container)
#####################################################################

my @nodes = @{get_ws($tmp)->{floating_nodes}};
my $rect = $nodes[0]->{rect};

# we compare the width with ± 20 pixels for borders
cmp_ok($rect->{width}, '>', 500-20, 'width now > 480');
cmp_ok($rect->{width}, '<', 500+20, 'width now < 520');
# we compare the height with ± 40 pixels for decorations
cmp_ok($rect->{height}, '>', 90-40, 'width now > 50');
cmp_ok($rect->{height}, '<', 90+40, 'width now < 130');

done_testing;
