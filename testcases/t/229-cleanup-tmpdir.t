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
use File::Temp qw(tempfile tempdir);
use X11::XCB qw(:all);

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

################################################################################
# Regression: with a socket path outside of tmpdir, i3 would delete the tmpdir
# prematurely and could not use it for storing the restart layout.
################################################################################

# Set XDG_RUNTIME_DIR to a temp directory so that i3 will create its directory
# in there and we’ll be able to find it. Necessary because we cannot deduce the
# temp directory from the socket path (which we explicitly set).
$ENV{XDG_RUNTIME_DIR} = tempdir(CLEANUP => 1);

my ($outfh, $outname) = tempfile('/tmp/i3-socket.XXXXXX', UNLINK => 1);
$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

ipc-socket $outname
EOT

$pid = launch_with_config($config, dont_add_socket_path => 1, dont_create_temp_dir => 1);
$socketpath = get_socket_path(0);

sub get_config_path {
    my $atom = $x->atom(name => 'I3_CONFIG_PATH');
    my $cookie = $x->get_property(0, $x->get_root_window(), $atom->id, GET_PROPERTY_TYPE_ANY, 0, 256);
    my $reply = $x->get_property_reply($cookie->{sequence});
    return $reply->{value};
}

my ($outfh2, $outname2) = tempfile('/tmp/i3-socket.XXXXXX', UNLINK => 1);
my $config_path = get_config_path();
open(my $configfh, '>', $config_path);
say $configfh <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
ipc-socket $outname2
EOT
close($configfh);
$tmpdir = $ENV{XDG_RUNTIME_DIR} . '/i3';

ok(-d $tmpdir, "tmpdir $tmpdir exists");

# Clear the error logfile. The testsuite runs in an environment where RandR is
# not supported, so there always is a message about xinerama in the error
# logfile.
@errorlogfiles = <$tmpdir/errorlog.*>;
for my $fn (@errorlogfiles) {
    open(my $fh, '>', $fn);
    close($fh);
}

my $tmp = fresh_workspace;
my $win = open_window;
cmd 'border none';
my ($nodes, $focus) = get_ws_content($tmp);
is($nodes->[0]->{border}, 'none', 'border is none');

cmd 'restart';

# The socket path will be different, and we use that for checking whether i3 has restarted yet.
while (get_socket_path(0) eq $socketpath) {
    sleep 0.1;
}

$new_tmpdir = $ENV{XDG_RUNTIME_DIR} . '/i3';

does_i3_live;

($nodes, $focus) = get_ws_content($tmp);
is($nodes->[0]->{border}, 'none', 'border still none after restart');

exit_gracefully($pid);

close($outfh);
close($outfh2);

done_testing;
