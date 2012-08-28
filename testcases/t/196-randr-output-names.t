#!perl
# vim:ts=4:sw=4:expandtab
# Verify that i3 allows strange RandR output names such as DVI-I_1/digital.
# Ticket: #785
# Bug still in: 4.2-256-ga007283
use i3test i3_autostart => 0;
use File::Temp qw(tempfile);

my ($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

workspace 2 output DVI-I_1/digital
EOT

my $output = qx(../i3 -C -c $filename);
unlike($output, qr/ERROR/, 'no errors in i3 -C');

close($fh);

done_testing;
