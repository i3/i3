#!perl
# vim:ts=4:sw=4:expandtab
# Beware that this test uses workspace 9 to perform some tests (it expects
# the workspace to be empty).
# TODO: skip it by default?

use Test::More tests => 10;
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
    sleep(0.5);
    return X11::XCB::Connection->input_focus;
}

$focus = X11::XCB::Connection->input_focus;
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

$sock->write(i3test::format_ipc_command("m12"));
$sock->write(i3test::format_ipc_command("t"));
$sock->write(i3test::format_ipc_command("m13"));
$sock->write(i3test::format_ipc_command("12"));
$sock->write(i3test::format_ipc_command("13"));
ok(1, "Still living");
