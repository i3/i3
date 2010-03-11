#!perl
# vim:ts=4:sw=4:expandtab

use Test::More tests => 8;
use Test::Exception;
use Data::Dumper;
use JSON::XS;
use List::MoreUtils qw(all);
use FindBin;
use lib "$FindBin::Bin/lib";
use i3test;

BEGIN {
    use_ok('IO::Socket::UNIX') or BAIL_OUT('Cannot load IO::Socket::UNIX');
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

my $sock = IO::Socket::UNIX->new(Peer => '/tmp/i3-ipc.sock');
isa_ok($sock, 'IO::Socket::UNIX');

####################
# Request workspaces
####################

# message type 1 is GET_WORKSPACES
my $message = "i3-ipc" . pack("LL", 0, 1);
$sock->write($message);

#######################################
# Test the reply format for correctness
#######################################

# The following lines duplicate functionality from recv_ipc_command
# to have it included in the test-suite.
my $buffer;
$sock->read($buffer, length($message));
is(substr($buffer, 0, length("i3-ipc")), "i3-ipc", "ipc message received");
my ($len, $type) = unpack("LL", substr($buffer, 6));
is($type, 1, "correct reply type");

# read the payload
$sock->read($buffer, $len);
my $workspaces;

#########################
# Actually test the reply
#########################

lives_ok { $workspaces = decode_json($buffer) } 'JSON could be decoded';

ok(@{$workspaces} > 0, "More than zero workspaces found");

my $name_exists = all { defined($_->{name}) } @{$workspaces};
ok($name_exists, "All workspaces have a name");

diag( "Testing i3, Perl $], $^X" );
