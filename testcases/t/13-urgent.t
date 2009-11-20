#!perl
# vim:ts=4:sw=4:expandtab
# Beware that this test uses workspace 9 to perform some tests (it expects
# the workspace to be empty).
# TODO: skip it by default?

use Test::More tests => 9;
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
# Create two windows and put them in stacking mode
#####################################################################

my $top = i3test::open_standard_window($x);
sleep 0.25;
my $bottom = i3test::open_standard_window($x);
sleep 0.25;

$sock->write(i3test::format_ipc_command("s"));
sleep 0.25;

#####################################################################
# Add the urgency hint, switch to a different workspace and back again
#####################################################################
$top->add_hint('urgency');
sleep 1;

$sock->write(i3test::format_ipc_command("1"));
sleep 0.25;
$sock->write(i3test::format_ipc_command("9"));
sleep 0.25;
$sock->write(i3test::format_ipc_command("1"));
sleep 0.25;

my $std = i3test::open_standard_window($x);
sleep 0.25;
$std->add_hint('urgency');
sleep 1;
