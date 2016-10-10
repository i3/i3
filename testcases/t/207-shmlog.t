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
use i3test i3_autostart => 0;
use IPC::Run qw(run);
use File::Temp;

################################################################################
# 1: test that shared memory logging does not work yet
################################################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

# NB: launch_with_config sets --shmlog-size=0 because the logfile gets
# redirected via stdout redirection anyways.
my $pid = launch_with_config($config);

my $stdout;
my $stderr;
run [ 'i3-dump-log' ],
    '>', \$stdout,
    '2>', \$stderr;

like($stderr, qr#^i3-dump-log: ERROR: i3 is running, but SHM logging is not enabled\.#,
    'shm logging not enabled');

################################################################################
# 2: enable shared memory logging and verify new content shows up
################################################################################

cmd 'shmlog on';

my $random_nop = mktemp('nop.XXXXXX');
cmd "nop $random_nop";

run [ 'i3-dump-log' ],
    '>', \$stdout,
    '2>', \$stderr;

like($stdout, qr#$random_nop#, 'random nop found in shm log');
like($stderr, qr#^$#, 'stderr empty');

################################################################################
# 3: change size of the shared memory log buffer and verify old content is gone
################################################################################

cmd 'shmlog ' . (23 * 1024 * 1024);

run [ 'i3-dump-log' ],
    '>', \$stdout,
    '2>', \$stderr;

unlike($stdout, qr#$random_nop#, 'random nop not found in shm log');
like($stderr, qr#^$#, 'stderr empty');

################################################################################
# 4: disable logging and verify it no longer works
################################################################################

cmd 'shmlog off';

run [ 'i3-dump-log' ],
    '>', \$stdout,
    '2>', \$stderr;

like($stderr, qr#^i3-dump-log: ERROR: i3 is running, but SHM logging is not enabled\.#,
    'shm logging not enabled');

exit_gracefully($pid);

done_testing;
