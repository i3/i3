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
# Verify that i3 allows strange RandR output names such as DVI-I_1/digital.
# Ticket: #785
# Bug still in: 4.2-256-ga007283
use i3test i3_autostart => 0;
use File::Temp qw(tempfile);

my ($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

workspace 2 output DVI-I_1/digital
EOT

my $output = qx(i3 -C -c $filename);
unlike($output, qr/ERROR/, 'no errors in i3 -C');

close($fh);

done_testing;
