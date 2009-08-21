#!perl
# vim:ts=4:sw=4:expandtab
# Checks if the focus is correctly restored, when creating a floating client
# over an unfocused tiling client and destroying the floating one again.

use Test::More tests => 5;
use Test::Deep;
use X11::XCB qw(:all);
use Data::Dumper;
use Time::HiRes qw(sleep);
use FindBin;
use lib "$FindBin::Bin/lib";
use i3test;

BEGIN {
    use_ok('IO::Socket::UNIX') or BAIL_OUT('Cannot load IO::Socket::UNIX');
    use_ok('X11::XCB::Window') or BAIL_OUT('Could not load X11::XCB::Window');
}

X11::XCB::Connection->connect(':0');

my $sock = IO::Socket::UNIX->new(Peer => '/tmp/i3-ipc.sock');
isa_ok($sock, 'IO::Socket::UNIX');

# Switch to the nineth workspace
$sock->write(i3test::format_ipc_command("9"));

sleep(0.25);


my $tiled_left = i3test::open_standard_window;
my $tiled_right = i3test::open_standard_window;

sleep(0.25);

$sock->write(i3test::format_ipc_command("ml"));

# Get input focus before creating the floating window
my $focus = X11::XCB::Connection->input_focus;

# Create a floating window which is smaller than the minimum enforced size of i3
my $window = X11::XCB::Window->new(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 1, 1, 30, 30],
    background_color => '#C0C0C0',
    type => 'utility',
);

isa_ok($window, 'X11::XCB::Window');

$window->create;
$window->map;

sleep(0.25);

$window->unmap;

sleep(0.25);

is(X11::XCB::Connection->input_focus, $focus, 'Focus correctly restored');

diag( "Testing i3, Perl $], $^X" );
