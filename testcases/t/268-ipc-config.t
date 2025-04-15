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
# Verifies that the config file is returned raw via the IPC interface.
# Ticket: #2856
# Bug still in: 4.13-133-ge4da07e7
use i3test i3_autostart => 0;
use File::Temp qw(tempdir);

my $tmpdir = tempdir(CLEANUP => 1);
my $socketpath = $tmpdir . "/config.sock";
ok(! -e $socketpath, "$socketpath does not exist yet");

my $config = <<'EOT';
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

nop foo \
continued

set $var normal title
for_window [title="$vartest"] border none
EOT

$config .= "ipc-socket $socketpath";

my $pid = launch_with_config($config, dont_add_socket_path => 1, dont_create_temp_dir => 1);
get_socket_path(0);
my $i3 = i3(get_socket_path());
$i3->connect->recv;

my $cv = AnyEvent->condvar;
my $timer = AnyEvent->timer(after => 0.5, interval => 0, cb => sub { $cv->send(0); });

my $last_config = $i3->get_config()->recv;
chomp($last_config->{config});
is($last_config->{config}, $config,
   'received config is not equal to written config');

exit_gracefully($pid);

done_testing;
