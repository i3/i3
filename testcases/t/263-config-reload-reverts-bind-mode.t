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
# Verifies that reloading the config reverts to the default
# binding mode.
# Ticket: #2228
# Bug still in: 4.11-262-geb631ce
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

mode "othermode" {
}
EOT

cmd 'mode othermode';

my @events = events_for(
    sub { cmd 'reload' },
    'mode');

is(scalar @events, 1, 'Received 1 event');
is($events[0]->{change}, 'default', 'change is "default"');

done_testing;
