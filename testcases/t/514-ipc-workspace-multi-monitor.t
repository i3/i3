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
# Ticket: #990
# Bug still in: 4.5.1-23-g82b5978

use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

my $old_ws = get_ws(focused_ws);

my $focus = AnyEvent->condvar;
my @events = events_for(
    sub { cmd 'focus output right' },
    'workspace');

my $current_ws = get_ws(focused_ws);

is(scalar @events, 1, 'Received 1 event');
is($events[0]->{current}->{id}, $current_ws->{id}, 'Event gave correct current workspace');
is($events[0]->{old}->{id}, $old_ws->{id}, 'Event gave correct old workspace');

done_testing;
