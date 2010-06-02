#!perl
# vim:ts=4:sw=4:expandtab

use i3test tests => 6;
use X11::XCB qw(:all);
use Time::HiRes qw(sleep);
use Digest::SHA1 qw(sha1_base64);

BEGIN {
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

my $x = X11::XCB::Connection->new;

my $i3 = i3("/tmp/nestedcons");
my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

$i3->command('split h')->recv;

#####################################################################
# Create two windows and make sure focus switching works
#####################################################################

my $top = i3test::open_standard_window($x);
sleep 0.25;
my $mid = i3test::open_standard_window($x);
sleep 0.25;
my $bottom = i3test::open_standard_window($x);
sleep 0.25;

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

$focus = $x->input_focus;
is($focus, $bottom->id, "Latest window focused");

$focus = focus_after("prev h");
is($focus, $mid->id, "Middle window focused");

#####################################################################
# Now goto a mark which does not exist
#####################################################################

my $random_mark = sha1_base64(rand());

$focus = focus_after(qq|[con_mark="$random_mark"] focus|);
is($focus, $mid->id, "focus unchanged");

$i3->command("mark $random_mark")->recv;

$focus = focus_after("prev h");
is($focus, $top->id, "Top window focused");

$focus = focus_after(qq|[con_mark="$random_mark"] focus|);
is($focus, $mid->id, "goto worked");

