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
# Ensure IPC works by switching workspaces
#####################################################################

# Create a window so we can get a focus different from NULL
my $window = open_standard_window($x);
diag("window->id = " . $window->id);

sleep 0.25;

my $focus = $x->input_focus;
diag("old focus = $focus");

# Switch to another workspace
fresh_workspace;

my $new_focus = $x->input_focus;
isnt($focus, $new_focus, "Focus changed");

done_testing;
