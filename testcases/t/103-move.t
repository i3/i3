#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Beware that this test uses workspace 9 to perform some tests (it expects
# the workspace to be empty).
# TODO: skip it by default?

use i3test tests => 8;
use X11::XCB qw(:all);
use Time::HiRes qw(sleep);

BEGIN {
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

SKIP: {
    skip "Testcase not yet modified for new move concept", 7;

my $x = X11::XCB::Connection->new;

my $i3 = i3;

# Switch to the ninth workspace
$i3->command('9')->recv;

#####################################################################
# Create two windows and make sure focus switching works
#####################################################################

my $top = i3test::open_standard_window($x);
sleep(0.25);
my $mid = i3test::open_standard_window($x);
sleep(0.25);
my $bottom = i3test::open_standard_window($x);
sleep(0.25);

diag("top id = " . $top->id);
diag("mid id = " . $mid->id);
diag("bottom id = " . $bottom->id);

#
# Returns the input focus after sending the given command to i3 via IPC
# end sleeping for half a second to make sure i3 reacted
#
sub focus_after {
    my $msg = shift;

    $i3->command($msg)->recv;
    return $x->input_focus;
}

my $focus = $x->input_focus;
is($focus, $bottom->id, "Latest window focused");

$focus = focus_after("ml");
is($focus, $bottom->id, "Right window still focused");

$focus = focus_after("h");
is($focus, $mid->id, "Middle window focused");

#####################################################################
# Now move to the top window, move right, then move left again
# (e.g., does i3 remember the focus in the last container?)
#####################################################################

$focus = focus_after("k");
is($focus, $top->id, "Top window focused");

$focus = focus_after("l");
is($focus, $bottom->id, "Right window focused");

$focus = focus_after("h");
is($focus, $top->id, "Top window focused");

#####################################################################
# Move window cross-workspace
#####################################################################

for my $cmd (qw(m12 t m13 12 13)) {
    $i3->command($cmd)->recv;
}
ok(1, "Still living");
}
