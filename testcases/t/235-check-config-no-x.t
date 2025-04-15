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
# Check whether the -C option works without a display and doesn't
# accidentally start the nagbar.
#
use i3test i3_autostart => 0;
use File::Temp qw(tempfile);

my ($cfg, $ret, $out);

sub check_config {
    my ($config) = @_;
    my ($fh, $tmpfile) = tempfile(UNLINK => 1);
    print $fh $config;
    my $output = qx(DISPLAY= i3 -C -c $tmpfile 2>&1);
    my $retval = $?;
    $fh->flush;
    close($fh);
    return ($retval >> 8, $output);
}

################################################################################
# 1: test with a bogus configuration file
################################################################################

$cfg = <<EOT;
i_am_an_unknown_config option
EOT

($ret, $out) = check_config($cfg);
is($ret, 1, "exit code == 1");
like($out, qr/ERROR: *CONFIG: *[Ee]xpected.*tokens/, 'bogus config file');

################################################################################
# 2: test with a valid configuration file
################################################################################

$cfg = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

($ret, $out) = check_config($cfg);
is($ret, 0, "exit code == 0");
is($out, "", 'valid config file');

################################################################################
# 3: test duplicate keybindings
################################################################################

$cfg = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
bindsym Shift+a nop 1
bindsym Shift+a nop 2
EOT

($ret, $out) = check_config($cfg);
is($ret, 1, "exit code == 1");
like($out, qr/ERROR: *Duplicate keybinding in config file/, 'duplicate keybindings');

################################################################################
# 4: test no duplicate keybindings
################################################################################

$cfg = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
bindsym Shift+a nop 1
EOT

($ret, $out) = check_config($cfg);
is($ret, 0, "exit code == 0");
is($out, "", 'valid config file');

done_testing;
