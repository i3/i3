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
# Ensures that calling i3 with the -C switch works (smoke test).
# Ticket: #2144
use i3test i3_autostart => 0;
use POSIX ":sys_wait_h";
use Time::HiRes qw(sleep);

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

invalid
EOT

my $exit_code = launch_with_config($config, validate_config => 1);
isnt($exit_code, 0, 'i3 exited with an error code');

my $log = get_i3_log;

# We don't care so much about the output (there are tests for this), but rather
# that we got correct output at all instead of, e.g., a segfault.
ok($log =~ /Expected one of these tokens/, 'an invalid config token was found');

done_testing;
