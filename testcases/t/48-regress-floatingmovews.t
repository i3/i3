#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression test for correct focus behaviour when moving a floating con to
# another workspace.
#
use X11::XCB qw(:all);
use i3test;

BEGIN {
    use_ok('X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;

my $tmp = fresh_workspace;

# open a tiling window on the first workspace
open_standard_window($x);
#sleep 0.25;
my $first = get_focused($tmp);

# on a different ws, open a floating window
my $otmp = fresh_workspace;
open_standard_window($x);
#sleep 0.25;
my $float = get_focused($otmp);
cmd 'mode toggle';
#sleep 0.25;

# move the floating con to first workspace
cmd "move workspace $tmp";
#sleep 0.25;

# switch to the first ws and check focus
is(get_focused($tmp), $float, 'floating client correctly focused');

done_testing;
