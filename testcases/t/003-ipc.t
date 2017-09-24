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
