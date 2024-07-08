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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Test that i3 will not hang if a connected client stops reading from its
# subscription socket and that the client is killed after a delay.
# Ticket: #2999
# Bug still in: 4.15-180-g715cea61
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
# Set the timeout to 500ms to reduce the duration of this test.
ipc_kill_timeout 500
EOT

# Manually connect to i3 so that we can choose to not read events
use IO::Socket::UNIX;
my $sock = IO::Socket::UNIX->new(Peer => get_socket_path());
my $magic = "i3-ipc";
my $payload = '["workspace"]';
my $message = $magic . pack("LL", length($payload), 2) . $payload;
print $sock $message;

# Constantly switch between 2 workspaces to generate events.
fresh_workspace;
open_window;
fresh_workspace;
open_window;

eval {
    local $SIG{ALRM} = sub { die "Timeout\n" };
    # 500 is an arbitrarily large number to make sure that the socket becomes
    # non-writeable.
    for (my $i = 0; $i < 500; $i++) {
        alarm 1;
        cmd 'workspace back_and_forth';
        alarm 0;
    }
};
ok(!$@, 'i3 didn\'t hang');

# Wait for connection timeout
sleep 1;

use IO::Select;
my $s = IO::Select->new($sock);
my $reached_eof = 0;
while ($s->can_read(0.05)) {
    if (read($sock, my $buffer, 100) == 0) {
        $reached_eof = 1;
        last;
    }
}
ok($reached_eof, 'socket connection closed');

close $sock;
done_testing;
