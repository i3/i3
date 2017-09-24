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
#
# Test to see if i3 combines the geometry of all children in a split container
# when setting the split container to floating
#
use i3test;

my $tmp = fresh_workspace;

open_window;
cmd 'split v';

#####################################################################
# open a window with 200x80
#####################################################################

my $first = open_window({
        rect => [ 0, 0, 200, 80],
        background_color => '#FF0000',
    });

cmd 'split h';

#####################################################################
# Open a second window with 300x90
#####################################################################

my $second = open_window({
        rect => [ 0, 0, 300, 90],
        background_color => '#00FF00',
    });

#####################################################################
# Set the parent to floating
#####################################################################
cmd 'nop setting floating';
cmd 'focus parent';
cmd 'floating enable';

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
