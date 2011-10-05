#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
# Tests if the various ipc_socket_path options are correctly handled
#
use i3test;
use File::Temp qw(tempfile tempdir);
use POSIX qw(getuid);
use v5.10;

#####################################################################
# default case: socket will be created in /tmp/i3-<username>/ipc-socket.<pid>
#####################################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

# ensure XDG_RUNTIME_DIR is not set
delete $ENV{XDG_RUNTIME_DIR};
my $pid = launch_with_config($config, 1);

my $folder = "/tmp/i3-" . getpwuid(getuid());
ok(-d $folder, "folder $folder exists");
my $socketpath = "$folder/ipc-socket." . $pid;
ok(-S $socketpath, "file $socketpath exists and is a socket");

exit_gracefully($pid);

#####################################################################
# XDG_RUNTIME_DIR case: socket gets created in $XDG_RUNTIME_DIR/i3/ipc-socket.<pid>
#####################################################################

my $rtdir = tempdir(CLEANUP => 1);

ok(! -e "$rtdir/i3", "$rtdir/i3 does not exist yet");

$ENV{XDG_RUNTIME_DIR} = $rtdir;

$pid = launch_with_config($config, 1);

ok(-d "$rtdir/i3", "$rtdir/i3 exists and is a directory");
$socketpath = "$rtdir/i3/ipc-socket." . $pid;
ok(-S $socketpath, "file $socketpath exists and is a socket");

exit_gracefully($pid);

#####################################################################
# configuration file case: socket gets placed whereever we specify
#####################################################################

my $tmpdir = tempdir(CLEANUP => 1);
$socketpath = $tmpdir . "/config.sock";
ok(! -e $socketpath, "$socketpath does not exist yet");

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
ipc-socket $socketpath
EOT

$pid = launch_with_config($config, 1);

ok(-S $socketpath, "file $socketpath exists and is a socket");

exit_gracefully($pid);

done_testing;
