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
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
use i3test;
use IPC::Run qw(run);
use File::Temp;

################################################################################
# 1: test that shared memory logging does not work yet
################################################################################

# NB: launch_with_config (called in i3test) sets --shmlog-size=0 because the
# logfile gets redirected via stdout redirection anyways.

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

cmd 'shmlog ' . (1 * 1024 * 1024);

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

done_testing;
