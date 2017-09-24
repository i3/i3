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

my $tmp = fresh_workspace;

#####################################################################
# Create two windows and make sure focus switching works
#####################################################################

# Change mode of the container to "default" for following tests
cmd 'layout default';
cmd 'split v';

my $top = open_window;
my $mid = open_window;
my $bottom = open_window;

#
# Returns the input focus after sending the given command to i3 via IPC
# end sleeping for half a second to make sure i3 reacted
#
sub focus_after {
    my $msg = shift;

    cmd $msg;
    return $x->input_focus;
}

my $focus = $x->input_focus;
is($focus, $bottom->id, "Latest window focused");

$focus = focus_after('focus up');
is($focus, $mid->id, "Middle window focused");

$focus = focus_after('focus up');
is($focus, $top->id, "Top window focused");

#####################################################################
# Test focus wrapping
#####################################################################

$focus = focus_after('focus up');
is($focus, $bottom->id, "Bottom window focused (wrapping to the top works)");

$focus = focus_after('focus down');
is($focus, $top->id, "Top window focused (wrapping to the bottom works)");

#####################################################################
# Test focus is only successful if there was a window that could be
# matched.
#####################################################################

my $result = cmd '[con_mark=__does_not_exist] focus';
is($result->[0]->{success}, 0, 'focus is unsuccessful if no window was matched');

#####################################################################

done_testing;
