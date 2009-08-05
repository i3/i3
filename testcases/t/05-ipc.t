#!perl
# vim:ts=4:sw=4:expandtab

use Test::More tests => 4;
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


#####################################################################
# Ensure IPC works by switching workspaces
#####################################################################

# Switch to the first workspace to get a clean testing environment
$sock->write(i3test::format_ipc_command("1"));

sleep(0.25);

# Create a window so we can get a focus different from NULL
my $window = i3test::open_standard_window;
diag("window->id = " . $window->id);

sleep(0.25);

my $focus = X11::XCB::Connection->input_focus;
diag("old focus = $focus");

# Switch to the nineth workspace
$sock->write(i3test::format_ipc_command("9"));

sleep(0.25);

my $new_focus = X11::XCB::Connection->input_focus;
isnt($focus, $new_focus, "Focus changed");

diag( "Testing i3, Perl $], $^X" );
