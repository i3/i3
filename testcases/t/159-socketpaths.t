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
# Tests if the various ipc_socket_path options are correctly handled
#
use i3test i3_autostart => 0;
use File::Temp qw(tempfile tempdir);
use File::Basename;
use POSIX qw(getuid);
use v5.10;

#####################################################################
# default case: socket will be created in /tmp/i3-<username>/ipc-socket.<pid>
#####################################################################

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

# ensure XDG_RUNTIME_DIR is not set
delete $ENV{XDG_RUNTIME_DIR};

my $pid = launch_with_config($config, dont_add_socket_path => 1, dont_create_temp_dir => 1);
my $socketpath = get_socket_path(0);
my $folder = "/tmp/i3-" . getpwuid(getuid());
like(dirname($socketpath), qr/^$folder/, 'temp directory matches expected pattern');
$folder = dirname($socketpath);

ok(-d $folder, "folder $folder exists");
$socketpath = "$folder/ipc-socket." . $pid;
ok(-S $socketpath, "file $socketpath exists and is a socket");

exit_gracefully($pid);

#####################################################################
# XDG_RUNTIME_DIR case: socket gets created in $XDG_RUNTIME_DIR/i3/ipc-socket.<pid>
#####################################################################

my $rtdir = tempdir(CLEANUP => 1);

ok(! -e "$rtdir/i3", "$rtdir/i3 does not exist yet");

$ENV{XDG_RUNTIME_DIR} = $rtdir;

$pid = launch_with_config($config, dont_add_socket_path => 1, dont_create_temp_dir => 1);

ok(-d "$rtdir/i3", "$rtdir/i3 exists and is a directory");
$socketpath = "$rtdir/i3/ipc-socket." . $pid;
ok(-S $socketpath, "file $socketpath exists and is a socket");

exit_gracefully($pid);

#####################################################################
# configuration file case: socket gets placed wherever we specify
#####################################################################

my $tmpdir = tempdir(CLEANUP => 1);
$socketpath = $tmpdir . "/config.sock";
ok(! -e $socketpath, "$socketpath does not exist yet");

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
ipc-socket $socketpath
EOT

$pid = launch_with_config($config, dont_add_socket_path => 1, dont_create_temp_dir => 1);

ok(-S $socketpath, "file $socketpath exists and is a socket");

exit_gracefully($pid);

done_testing;
