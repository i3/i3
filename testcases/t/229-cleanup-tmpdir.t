#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Makes sure i3 deletes its temporary directory when exiting.
# Ticket: #1253
# Bug still in: 4.7.2-186-g617afc6
use i3test i3_autostart => 0;
use File::Basename;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

# ensure XDG_RUNTIME_DIR is not set
delete $ENV{XDG_RUNTIME_DIR};

my $pid = launch_with_config($config, dont_add_socket_path => 1, dont_create_temp_dir => 1);
my $socketpath = get_socket_path(0);
my $tmpdir = dirname($socketpath);

ok(-d $tmpdir, "tmpdir $tmpdir exists");

# Clear the error logfile. The testsuite runs in an environment where RandR is
# not supported, so there always is a message about xinerama in the error
# logfile.
my @errorlogfiles = <$tmpdir/errorlog.*>;
for my $fn (@errorlogfiles) {
    open(my $fh, '>', $fn);
    close($fh);
}

exit_gracefully($pid);

ok(! -d $tmpdir, "tmpdir $tmpdir was cleaned up");
if (-d $tmpdir) {
    diag('contents = ' . Dumper(<$tmpdir/*>));
}

$pid = launch_with_config($config, dont_add_socket_path => 1, dont_create_temp_dir => 1);
$socketpath = get_socket_path(0);
$tmpdir = dirname($socketpath);

ok(-d $tmpdir, "tmpdir $tmpdir exists");

# Clear the error logfile. The testsuite runs in an environment where RandR is
# not supported, so there always is a message about xinerama in the error
# logfile.
@errorlogfiles = <$tmpdir/errorlog.*>;
for my $fn (@errorlogfiles) {
    open(my $fh, '>', $fn);
    close($fh);
}

diag('socket path before restarting is ' . $socketpath);

cmd 'restart';

# The socket path will be different, and we use that for checking whether i3 has restarted yet.
while (get_socket_path(0) eq $socketpath) {
    sleep 0.1;
}

my $new_tmpdir = dirname(get_socket_path());

does_i3_live;

# Clear the error logfile. The testsuite runs in an environment where RandR is
# not supported, so there always is a message about xinerama in the error
# logfile.
@errorlogfiles = <$new_tmpdir/errorlog.*>;
for my $fn (@errorlogfiles) {
    open(my $fh, '>', $fn);
    close($fh);
}

exit_gracefully($pid);

ok(! -d $tmpdir, "old tmpdir $tmpdir was cleaned up");
if (-d $tmpdir) {
    diag('contents = ' . Dumper(<$tmpdir/*>));
}

ok(! -d $new_tmpdir, "new tmpdir $new_tmpdir was cleaned up");
if (-d $new_tmpdir) {
    diag('contents = ' . Dumper(<$new_tmpdir/*>));
}


done_testing;
