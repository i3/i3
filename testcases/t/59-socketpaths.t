#!perl
# vim:ts=4:sw=4:expandtab
# !NO_I3_INSTANCE! will prevent complete-run.pl from starting i3
#
# Tests if the various ipc_socket_path options are correctly handled
#
use i3test;
use Cwd qw(abs_path);
use Proc::Background;
use File::Temp qw(tempfile tempdir);
use POSIX qw(getuid);
use v5.10;

# assuming we are run by complete-run.pl
my $i3_path = abs_path("../i3");

#####################################################################
# default case: socket will be created in /tmp/i3-<username>/ipc-socket.<pid>
#####################################################################

my ($fh, $tmpfile) = tempfile();
say $fh "font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1";
close($fh);

diag("Starting i3");
my $i3cmd = "unset XDG_RUNTIME_DIR; exec " . abs_path("../i3") . " -V -d all --disable-signalhandler -c $tmpfile >/dev/null 2>/dev/null";
my $process = Proc::Background->new($i3cmd);
sleep 1;

diag("pid = " . $process->pid);

my $folder = "/tmp/i3-" . getpwuid(getuid());
ok(-d $folder, "folder $folder exists");
my $socketpath = "$folder/ipc-socket." . $process->pid;
ok(-S $socketpath, "file $socketpath exists and is a socket");

exit_gracefully($process->pid, $socketpath);

sleep 0.25;

#####################################################################
# XDG_RUNTIME_DIR case: socket gets created in $XDG_RUNTIME_DIR/i3/ipc-socket.<pid>
#####################################################################

my $rtdir = tempdir(CLEANUP => 1);

ok(! -e "$rtdir/i3", "$rtdir/i3 does not exist yet");

$i3cmd = "export XDG_RUNTIME_DIR=$rtdir; exec " . abs_path("../i3") . " -V -d all --disable-signalhandler -c $tmpfile >/dev/null 2>/dev/null";
$process = Proc::Background->new($i3cmd);
sleep 1;

ok(-d "$rtdir/i3", "$rtdir/i3 exists and is a directory");
$socketpath = "$rtdir/i3/ipc-socket." . $process->pid;
ok(-S $socketpath, "file $socketpath exists and is a socket");

exit_gracefully($process->pid, $socketpath);

sleep 0.25;

#####################################################################
# configuration file case: socket gets placed whereever we specify
#####################################################################

my $tmpdir = tempdir(CLEANUP => 1);
$socketpath = $tmpdir . "/config.sock";
ok(! -e $socketpath, "$socketpath does not exist yet");

($fh, $tmpfile) = tempfile();
say $fh "font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1";
say $fh "ipc-socket $socketpath";
close($fh);

$i3cmd = "export XDG_RUNTIME_DIR=$rtdir; exec " . abs_path("../i3") . " -V -d all --disable-signalhandler -c $tmpfile >/dev/null 2>/dev/null";
$process = Proc::Background->new($i3cmd);
sleep 1;

ok(-S $socketpath, "file $socketpath exists and is a socket");

exit_gracefully($process->pid, $socketpath);

done_testing;
