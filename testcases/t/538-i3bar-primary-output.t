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
# Tests that i3bars configured to use the primary output do not have
# their output names canonicalized to something other than "primary".
# Ticket: #2948
# Bug still in: 4.14-93-ga3a7d04a
use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0P

bar {
    output primary
}
EOT

my $bars = i3->get_bar_config()->recv;
is(@$bars, 1, 'one bar configured');

my $bar_id = shift @$bars;

my $bar_config = i3->get_bar_config($bar_id)->recv;
is_deeply($bar_config->{outputs}, [ "primary" ], 'bar_config output is primary');

done_testing;
