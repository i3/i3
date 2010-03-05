#!perl
# vim:ts=4:sw=4:expandtab
# Beware that this test uses workspace 9 and 10 to perform some tests (it expects
# the workspace to be empty).
# TODO: skip it by default?

use Test::More tests => 5;
use Test::Deep;
use X11::XCB qw(:all);
use Data::Dumper;
use Time::HiRes qw(sleep);
use FindBin;
use Digest::SHA1 qw(sha1_base64);
use lib "$FindBin::Bin/lib";
use i3test;

BEGIN {
    use_ok('IO::Socket::UNIX') or BAIL_OUT('Cannot load IO::Socket::UNIX');
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

my $x = X11::XCB::Connection->new;

my $sock = IO::Socket::UNIX->new(Peer => '/tmp/i3-ipc.sock');
isa_ok($sock, 'IO::Socket::UNIX');

# Switch to the nineth workspace
$sock->write(i3test::format_ipc_command("9"));

sleep 0.25;

#####################################################################
# Create a parent window
#####################################################################

my $window = $x->root->create_child(
class => WINDOW_CLASS_INPUT_OUTPUT,
rect => [ 0, 0, 30, 30 ],
background_color => '#C0C0C0',
);

$window->name('Parent window');
$window->map;

sleep 0.25;

#########################################################################
# Switch workspace to 10 and open a child window. It should be positioned
# on workspace 9.
#########################################################################
$sock->write(i3test::format_ipc_command("10"));
sleep 0.25;

my $child = $x->root->create_child(
class => WINDOW_CLASS_INPUT_OUTPUT,
rect => [ 0, 0, 30, 30 ],
background_color => '#C0C0C0',
);

$child->name('Child window');
$child->client_leader($window);
$child->map;

sleep 0.25;

isnt($x->input_focus, $child->id, "Child window focused");

# Switch back
$sock->write(i3test::format_ipc_command("9"));
sleep 0.25;

is($x->input_focus, $child->id, "Child window focused");
