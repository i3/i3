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
# Tests resizing tiling containers
use i3test;

my ($left, $right);
my $tmp = fresh_workspace;

cmd 'split v';

my $top = open_window;
my $bottom = open_window;

diag("top = " . $top->id . ", bottom = " . $bottom->id);

is($x->input_focus, $bottom->id, 'Bottom window focused');

############################################################
# resize
############################################################

cmd 'resize grow up 10 px or 25 ppt';

my ($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.25, 'top window got only 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'bottom window got 75%');


############################################################
# split and check if the 'percent' factor is still correct
############################################################

cmd 'split h';

($nodes, $focus) = get_ws_content($tmp);

cmp_float($nodes->[0]->{percent}, 0.25, 'top window got only 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'bottom window got 75%');

############################################################
# checks that resizing within stacked/tabbed cons works
############################################################

$tmp = fresh_workspace;

cmd 'split v';

$top = open_window;
$bottom = open_window;

cmd 'split h';
cmd 'layout stacked';

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{percent}, 0.5, 'top window got 50%');
cmp_float($nodes->[1]->{percent}, 0.5, 'bottom window got 50%');

cmd 'resize grow up 10 px or 25 ppt';

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{percent}, 0.25, 'top window got 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'bottom window got 75%');

############################################################
# Checks that resizing in the parent's parent's orientation works.
# Take for example a horizontal workspace with one window on the left side and
# a v-split container with two windows on the right side. Focus is on the
# bottom right window, use 'resize left'.
############################################################

$tmp = fresh_workspace;

$left = open_window;
$right = open_window;

cmd 'split v';

$top = open_window;
$bottom = open_window;

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{percent}, 0.5, 'left window got 50%');
cmp_float($nodes->[1]->{percent}, 0.5, 'right window got 50%');

cmd 'resize grow left 10 px or 25 ppt';

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{percent}, 0.25, 'left window got 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'right window got 75%');

################################################################################
# Check that the resize grow/shrink width/height syntax works.
################################################################################

# Use two windows
$tmp = fresh_workspace;

$left = open_window;
$right = open_window;

cmd 'resize grow width 10 px or 25 ppt';

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{percent}, 0.25, 'left window got 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'right window got 75%');

# Now test it with four windows
$tmp = fresh_workspace;

open_window for (1..4);

cmd 'resize grow width 10 px or 25 ppt';

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{percent}, 0.166666666666667, 'first window got 16%');
cmp_float($nodes->[1]->{percent}, 0.166666666666667, 'second window got 16%');
cmp_float($nodes->[2]->{percent}, 0.166666666666667, 'third window got 16%');
cmp_float($nodes->[3]->{percent}, 0.50, 'fourth window got 50%');

# height should be a no-op in this situation
cmd 'resize grow height 10 px or 25 ppt';

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{percent}, 0.166666666666667, 'first window got 16%');
cmp_float($nodes->[1]->{percent}, 0.166666666666667, 'second window got 16%');
cmp_float($nodes->[2]->{percent}, 0.166666666666667, 'third window got 16%');
cmp_float($nodes->[3]->{percent}, 0.50, 'fourth window got 50%');

################################################################################
# Same but using pixels instead of ppt.
################################################################################

# Use two windows
$tmp = fresh_workspace;

$left = open_window;
$right = open_window;

($nodes, $focus) = get_ws_content($tmp);
my @widths = ($nodes->[0]->{rect}->{width}, $nodes->[1]->{rect}->{width});

cmd 'resize grow width 10 px';

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{rect}->{width}, $widths[0] - 10, 'left window is 10px smaller');
cmp_float($nodes->[1]->{rect}->{width}, $widths[1] + 10, 'right window is 10px larger');

# Now test it with four windows
$tmp = fresh_workspace;

open_window for (1..4);

($nodes, $focus) = get_ws_content($tmp);
my $width = $nodes->[0]->{rect}->{width};

cmd 'resize grow width 10 px';

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[3]->{rect}->{width}, $width + 10, 'last window is 10px larger');

################################################################################
# Same but for height
################################################################################

# Use two windows
$tmp = fresh_workspace;
cmd 'split v';

$left = open_window;
$right = open_window;

($nodes, $focus) = get_ws_content($tmp);
my @heights = ($nodes->[0]->{rect}->{height}, $nodes->[1]->{rect}->{height});

cmd 'resize grow height 10 px';

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{rect}->{height}, $heights[0] - 10, 'left window is 10px smaller');
cmp_float($nodes->[1]->{rect}->{height}, $heights[1] + 10, 'right window is 10px larger');

# Now test it with four windows
$tmp = fresh_workspace;
cmd 'split v';

open_window for (1..4);

($nodes, $focus) = get_ws_content($tmp);
my $height = $nodes->[0]->{rect}->{height};

cmd 'resize grow height 10 px';

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[3]->{rect}->{height}, $height + 10, 'last window is 10px larger');

################################################################################
# Check that we can grow tiled windows by pixels
################################################################################

$tmp = fresh_workspace;

$left = open_window;
$right = open_window;

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{rect}->{width}, 640, 'left window is 640px');
cmp_float($nodes->[1]->{rect}->{width}, 640, 'right window is 640px');

cmd 'resize grow left 10px';
($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{rect}->{width}, 630, 'left window is 630px');
cmp_float($nodes->[1]->{rect}->{width}, 650, 'right window is 650px');

################################################################################
# Check that we can shrink tiled windows by pixels
################################################################################

$tmp = fresh_workspace;

$left = open_window;
$right = open_window;

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{rect}->{width}, 640, 'left window is 640px');
cmp_float($nodes->[1]->{rect}->{width}, 640, 'right window is 640px');

cmd 'resize shrink left 10px';
($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{rect}->{width}, 650, 'left window is 650px');
cmp_float($nodes->[1]->{rect}->{width}, 630, 'right window is 630px');


################################################################################
# Check that we can shrink vertical tiled windows by pixels
################################################################################

$tmp = fresh_workspace;

cmd 'split v';

$top = open_window;
$bottom = open_window;

($nodes, $focus) = get_ws_content($tmp);
my @heights = ($nodes->[0]->{rect}->{height}, $nodes->[1]->{rect}->{height});

cmd 'resize grow up 10px';
($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{rect}->{height}, $heights[0] - 10, 'top window is 10px larger');
cmp_float($nodes->[1]->{rect}->{height}, $heights[1] + 10, 'bottom window is 10px smaller');

################################################################################
# Check that we can shrink vertical tiled windows by pixels
################################################################################

$tmp = fresh_workspace;

cmd 'split v';

$top = open_window;
$bottom = open_window;

($nodes, $focus) = get_ws_content($tmp);
my @heights = ($nodes->[0]->{rect}->{height}, $nodes->[1]->{rect}->{height});

cmd 'resize shrink up 10px';
($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{rect}->{height}, $heights[0] + 10, 'top window is 10px smaller');
cmp_float($nodes->[1]->{rect}->{height}, $heights[1] - 10, 'bottom window is 10px larger');

################################################################################
# Check that the resize grow/shrink width/height syntax works if a nested split
# was set on the container, but no sibling has been opened yet. See #2015.
################################################################################

$tmp = fresh_workspace;
$left = open_window;
$right = open_window;

cmd 'split h';
cmd 'resize grow width 10px or 25 ppt';

($nodes, $focus) = get_ws_content($tmp);
cmp_float($nodes->[0]->{percent}, 0.25, 'left window got 25%');
cmp_float($nodes->[1]->{percent}, 0.75, 'right window got 75%');

############################################################
# checks that resizing floating windows works
############################################################

$tmp = fresh_workspace;

$top = open_window;

cmd 'floating enable';

my @content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok(@content, '==', 1, 'one floating node on this ws');

# up
my $oldrect = $content[0]->{rect};

cmd 'resize grow up 10 px or 25 ppt';

@content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok($content[0]->{rect}->{y}, '<', $oldrect->{y}, 'y smaller than before');
cmp_ok($content[0]->{rect}->{y}, '==', $oldrect->{y} - 10, 'y exactly 10 px smaller');
cmp_ok($content[0]->{rect}->{x}, '==', $oldrect->{x}, 'x untouched');
cmp_ok($content[0]->{rect}->{height}, '>', $oldrect->{height}, 'height bigger than before');
cmp_ok($content[0]->{rect}->{height}, '==', $oldrect->{height} + 10, 'height exactly 10 px higher');
cmp_ok($content[0]->{rect}->{width}, '==', $oldrect->{width}, 'x untouched');

# up, but with a different amount of px
$oldrect = $content[0]->{rect};

cmd 'resize grow up 12 px or 25 ppt';

@content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok($content[0]->{rect}->{y}, '<', $oldrect->{y}, 'y smaller than before');
cmp_ok($content[0]->{rect}->{y}, '==', $oldrect->{y} - 12, 'y exactly 10 px smaller');
cmp_ok($content[0]->{rect}->{x}, '==', $oldrect->{x}, 'x untouched');
cmp_ok($content[0]->{rect}->{height}, '>', $oldrect->{height}, 'height bigger than before');
cmp_ok($content[0]->{rect}->{height}, '==', $oldrect->{height} + 12, 'height exactly 10 px higher');
cmp_ok($content[0]->{rect}->{width}, '==', $oldrect->{width}, 'x untouched');

# left
$oldrect = $content[0]->{rect};

cmd 'resize grow left 10 px or 25 ppt';

@content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok($content[0]->{rect}->{x}, '<', $oldrect->{x}, 'x smaller than before');
cmp_ok($content[0]->{rect}->{width}, '>', $oldrect->{width}, 'width bigger than before');

# right
$oldrect = $content[0]->{rect};

cmd 'resize grow right 10 px or 25 ppt';

@content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok($content[0]->{rect}->{x}, '==', $oldrect->{x}, 'x the same as before');
cmp_ok($content[0]->{rect}->{y}, '==', $oldrect->{y}, 'y the same as before');
cmp_ok($content[0]->{rect}->{width}, '>', $oldrect->{width}, 'width bigger than before');
cmp_ok($content[0]->{rect}->{height}, '==', $oldrect->{height}, 'height the same as before');

# down
$oldrect = $content[0]->{rect};

cmd 'resize grow down 10 px or 25 ppt';

@content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok($content[0]->{rect}->{x}, '==', $oldrect->{x}, 'x the same as before');
cmp_ok($content[0]->{rect}->{y}, '==', $oldrect->{y}, 'y the same as before');
cmp_ok($content[0]->{rect}->{height}, '>', $oldrect->{height}, 'height bigger than before');
cmp_ok($content[0]->{rect}->{width}, '==', $oldrect->{width}, 'width the same as before');

# grow width
$oldrect = $content[0]->{rect};

cmd 'resize grow width 10px or 10ppt';

@content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok($content[0]->{rect}->{x}, '==', $oldrect->{x}, 'x the same as before');
cmp_ok($content[0]->{rect}->{y}, '==', $oldrect->{y}, 'y the same as before');
cmp_ok($content[0]->{rect}->{height}, '==', $oldrect->{height}, 'height the same as before');
cmp_ok($content[0]->{rect}->{width}, '>', $oldrect->{width}, 'width bigger than before');

# shrink width
$oldrect = $content[0]->{rect};

cmd 'resize shrink width 10px or 10ppt';

@content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok($content[0]->{rect}->{x}, '==', $oldrect->{x}, 'x the same as before');
cmp_ok($content[0]->{rect}->{y}, '==', $oldrect->{y}, 'y the same as before');
cmp_ok($content[0]->{rect}->{height}, '==', $oldrect->{height}, 'height the same as before');
cmp_ok($content[0]->{rect}->{width}, '<', $oldrect->{width}, 'width smaller than before');

# grow height
$oldrect = $content[0]->{rect};

cmd 'resize grow height 10px or 10ppt';

@content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok($content[0]->{rect}->{x}, '==', $oldrect->{x}, 'x the same as before');
cmp_ok($content[0]->{rect}->{y}, '==', $oldrect->{y}, 'y the same as before');
cmp_ok($content[0]->{rect}->{height}, '>', $oldrect->{height}, 'height bigger than before');
cmp_ok($content[0]->{rect}->{width}, '==', $oldrect->{width}, 'width the same as before');

# shrink height
$oldrect = $content[0]->{rect};

cmd 'resize shrink height 10px or 10ppt';

@content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok($content[0]->{rect}->{x}, '==', $oldrect->{x}, 'x the same as before');
cmp_ok($content[0]->{rect}->{y}, '==', $oldrect->{y}, 'y the same as before');
cmp_ok($content[0]->{rect}->{height}, '<', $oldrect->{height}, 'height smaller than before');
cmp_ok($content[0]->{rect}->{width}, '==', $oldrect->{width}, 'width the same as before');

################################################################################
# Check that resizing with criteria works
################################################################################

$tmp = fresh_workspace;

$left = open_floating_window;
$right = open_floating_window;

sub get_floating_rect {
    my ($window_id) = @_;

    my $floating_nodes = get_ws($tmp)->{floating_nodes};
    for my $floating_node (@$floating_nodes) {
        # Get all the windows within that floating container
        my @window_ids = map { $_->{window} } @{$floating_node->{nodes}};
        if ($window_id ~~ @window_ids) {
            return $floating_node->{rect};
        }
    }

    return undef;
}

# focus is on the right window, so we resize the left one using criteria
my $leftold = get_floating_rect($left->id);
my $rightold = get_floating_rect($right->id);
cmd '[id="' . $left->id . '"] resize grow height 10px or 10ppt';

my $leftnew = get_floating_rect($left->id);
my $rightnew = get_floating_rect($right->id);
is($rightnew->{height}, $rightold->{height}, 'height of right container unchanged');
is($leftnew->{height}, $leftold->{height} + 10, 'height of left container changed');

done_testing;
