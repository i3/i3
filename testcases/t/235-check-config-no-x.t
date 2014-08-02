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
# Check whether the -C option works without a display and doesn't
# accidentally start the nagbar.
#
use i3test i3_autostart => 0;
use File::Temp qw(tempfile);

sub check_config {
    my ($config) = @_;
    my ($fh, $tmpfile) = tempfile(UNLINK => 1);
    print $fh $config;
    my $output = qx(DISPLAY= ../i3 -C -c $tmpfile 2>&1);
    my $retval = $?;
    $fh->flush;
    close($fh);
    return ($retval >> 8, $output);
}

################################################################################
# 1: test with a bogus configuration file
################################################################################

my $cfg = <<EOT;
# i3 config file (v4)
i_am_an_unknown_config option
EOT

my ($ret, $out) = check_config($cfg);
is($ret, 1, "exit code == 1");
like($out, qr/ERROR: *CONFIG: *[Ee]xpected.*tokens/, 'bogus config file');

################################################################################
# 2: test with a valid configuration file
################################################################################

my $cfg = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my ($ret, $out) = check_config($cfg);
is($ret, 0, "exit code == 0");
is($out, "", 'valid config file');

done_testing;
