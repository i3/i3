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
use File::Temp;

my $tmp = fresh_workspace;

cmd 'split h';

#####################################################################
# Create two windows and make sure focus switching works
#####################################################################

my $top = open_window;
my $mid = open_window;
my $bottom = open_window;

#
# Returns the input focus after sending the given command to i3 via IPC
# and syncing with i3
#
sub focus_after {
    my $msg = shift;

    cmd $msg;
    return $x->input_focus;
}

my $focus = $x->input_focus;
is($focus, $bottom->id, "Latest window focused");

$focus = focus_after('focus left');
is($focus, $mid->id, "Middle window focused");

#####################################################################
# Now goto a mark which does not exist
#####################################################################

my $random_mark = mktemp('mark.XXXXXX');

$focus = focus_after(qq|[con_mark="$random_mark"] focus|);
is($focus, $mid->id, "focus unchanged");

cmd "mark $random_mark";

$focus = focus_after('focus left');
is($focus, $top->id, "Top window focused");

$focus = focus_after(qq|[con_mark="$random_mark"] focus|);
is($focus, $mid->id, "goto worked");

# check that we can specify multiple criteria

$focus = focus_after('focus left');
is($focus, $top->id, "Top window focused");

$focus = focus_after(qq|[con_mark="$random_mark" con_mark="$random_mark"] focus|);
is($focus, $mid->id, "goto worked");

#####################################################################
# Set the same mark multiple times and see if focus works correctly
#####################################################################

$focus = focus_after('focus left');
is($focus, $top->id, "Top window focused");

cmd "mark $random_mark";

$focus = focus_after(qq|[con_mark="$random_mark"] focus|);
is($focus, $top->id, "focus unchanged after goto");

$focus = focus_after('focus right');
is($focus, $mid->id, "mid window focused");

$focus = focus_after(qq|[con_mark="$random_mark"] focus|);
is($focus, $top->id, "goto worked");

#####################################################################
# Check whether the focus command will switch to a different
# workspace if necessary
#####################################################################

my $tmp2 = fresh_workspace;

is(focused_ws(), $tmp2, 'tmp2 now focused');

cmd qq|[con_mark="$random_mark"] focus|;

is(focused_ws(), $tmp, 'tmp now focused');

done_testing;
