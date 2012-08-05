#!perl
# vim:ts=4:sw=4:expandtab
# Verifies that we can get the version number of i3 via IPC.
use i3test;

my $i3 = i3(get_socket_path());
$i3->connect->recv;
# We explicitly send the version message because AnyEvent::I3’s 'version' sugar
# method has a fallback which tries to parse the version number from i3
# --version for older versions, and we want to avoid using that.
my $version = $i3->message(7, "")->recv;

# We need to change this when the major version changes (but we need to touch a
# lot of changes then anyways).
is($version->{major}, 4, 'major version is 4');

cmp_ok($version->{minor}, '>', 0, 'minor version > 0');

is(int($version->{minor}), $version->{minor}, 'minor version is an integer');
is(int($version->{patch}), $version->{patch}, 'patch version is an integer');
like($version->{human_readable}, qr/branch/, 'human readable version contains branch name');

done_testing;
