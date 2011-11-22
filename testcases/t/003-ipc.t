#!perl
# vim:ts=4:sw=4:expandtab

use i3test;

fresh_workspace;

#####################################################################
# Ensure IPC works by switching workspaces
#####################################################################

# Create a window so we can get a focus different from NULL
my $window = open_window;

my $focus = $x->input_focus;

# Switch to another workspace
fresh_workspace;

sync_with_i3;
my $new_focus = $x->input_focus;
isnt($focus, $new_focus, "Focus changed");

done_testing;
