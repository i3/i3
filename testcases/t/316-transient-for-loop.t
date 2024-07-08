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
# Test that i3 does not get stuck in an endless loop between two windows that
# set transient_for for each other.
# Ticket: #4404
# Bug still in: 4.20-69-g43e805a00
#
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

popup_during_fullscreen smart;
EOT

my $fs = open_window;
cmd 'fullscreen enable';

my $w1 = open_window({ dont_map => 1 });
my $w2 = open_window({ dont_map => 1 });

$w1->transient_for($w2);
$w2->transient_for($w1);
$w1->map;
$w2->map;

does_i3_live;

done_testing;
