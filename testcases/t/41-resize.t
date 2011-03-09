#!perl
# vim:ts=4:sw=4:expandtab
# Tests resizing tiling containers
use i3test;
use X11::XCB qw(:all);

BEGIN {
    use_ok('X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;

my $tmp = fresh_workspace;

cmd 'split v';

my $top = open_standard_window($x);
sleep 0.25;
my $bottom = open_standard_window($x);
sleep 0.25;

diag("top = " . $top->id . ", bottom = " . $bottom->id);

is($x->input_focus, $bottom->id, 'Bottom window focused');

############################################################
# resize
############################################################

cmd 'resize grow up 10 px or 25 ppt';

my ($nodes, $focus) = get_ws_content($tmp);

is($nodes->[0]->{percent}, 0.25, 'top window got only 25%');
is($nodes->[1]->{percent}, 0.75, 'bottom window got 75%');


############################################################
# split and check if the 'percent' factor is still correct
############################################################

cmd 'split h';

($nodes, $focus) = get_ws_content($tmp);

is($nodes->[0]->{percent}, 0.25, 'top window got only 25%');
is($nodes->[1]->{percent}, 0.75, 'bottom window got 75%');

done_testing;
