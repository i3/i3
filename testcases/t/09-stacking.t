#!perl
# vim:ts=4:sw=4:expandtab
# Beware that this test uses workspace 9 to perform some tests (it expects
# the workspace to be empty).
# TODO: skip it by default?

use Test::More tests => 24;
use Test::Deep;
use X11::XCB qw(:all);
use Data::Dumper;
use Time::HiRes qw(sleep);
use FindBin;
use lib "$FindBin::Bin/lib";
use i3test;

BEGIN {
    use_ok('IO::Socket::UNIX') or BAIL_OUT('Cannot load IO::Socket::UNIX');
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

X11::XCB::Connection->connect(':0');

my $sock = IO::Socket::UNIX->new(Peer => '/tmp/i3-ipc.sock');
isa_ok($sock, 'IO::Socket::UNIX');

# Switch to the nineth workspace
$sock->write(i3test::format_ipc_command("9"));

sleep(0.25);

#####################################################################
# Create two windows and make sure focus switching works
#####################################################################

my $top = i3test::open_standard_window;
sleep(0.25);
my $mid = i3test::open_standard_window;
sleep(0.25);
my $bottom = i3test::open_standard_window;
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

    $sock->write(i3test::format_ipc_command($msg));
    sleep(0.25);
    return X11::XCB::Connection->input_focus;
}

$focus = X11::XCB::Connection->input_focus;
is($focus, $bottom->id, "Latest window focused");

$focus = focus_after("s");
is($focus, $bottom->id, "Last window still focused");

$focus = focus_after("k");
is($focus, $mid->id, "Middle window focused");

$focus = focus_after("k");
is($focus, $top->id, "Top window focused");

#####################################################################
# Test focus wrapping
#####################################################################

$focus = focus_after("k");
is($focus, $bottom->id, "Bottom window focused (wrapping to the top works)");

$focus = focus_after("j");
is($focus, $top->id, "Top window focused (wrapping to the bottom works)");

#####################################################################
# Restore of focus after moving windows out/into the stack
#####################################################################

$focus = focus_after("ml");
is($focus, $top->id, "Top window still focused (focus after moving)");

$focus = focus_after("h");
is($focus, $bottom->id, "Bottom window focused (focus after moving)");

my $new = i3test::open_standard_window;
sleep(0.25);

# By now, we have this layout:
# ----------------
# | mid    |
# | bottom | top
# | new    |
# ----------------

$focus = focus_after("l");
is($focus, $top->id, "Got top window");

$focus = focus_after("mh");
is($focus, $top->id, "Moved it into the stack");

$focus = focus_after("k");
is($focus, $new->id, "Window above is new");

$focus = focus_after("k");
is($focus, $bottom->id, "Window above is bottom");

$focus = focus_after("k");
is($focus, $mid->id, "Window above is mid");

$focus = focus_after("k");
is($focus, $top->id, "At top again");

$focus = focus_after("ml");
is($focus, $top->id, "Still at top, moved out");

$focus = focus_after("h");
is($focus, $mid->id, "At mid again");

$focus = focus_after("j");
is($focus, $bottom->id, "At bottom again");

$focus = focus_after("l");
is($focus, $top->id, "At top again");

$focus = focus_after("mh");
is($focus, $top->id, "Still at top, moved into");

$focus = focus_after("k");
is($focus, $bottom->id, "Window above is bottom");

$focus = focus_after("k");
is($focus, $mid->id, "Window above is mid");

