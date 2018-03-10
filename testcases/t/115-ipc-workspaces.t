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

use i3test;

my $old_ws = get_ws(focused_ws());

# We are switching to an empty workpspace from an empty workspace, so we expect
# to receive "init", "focus", and "empty".
my @events = events_for(
    sub { cmd 'workspace 2' },
    'workspace');

my $current_ws = get_ws(focused_ws());

is(scalar @events, 3, 'Received 3 events');
is($events[0]->{change}, 'init', 'First event has change = init');
is($events[0]->{current}->{id}, $current_ws->{id}, 'the "current" property contains the initted workspace con');

is($events[1]->{change}, 'focus', 'Second event has change = focus');
is($events[1]->{current}->{id}, $current_ws->{id}, 'the "current" property should contain the focused workspace con');
is($events[1]->{old}->{id}, $old_ws->{id}, 'the "old" property should contain the workspace con that was focused last');

is($events[2]->{change}, 'empty', 'Third event has change = empty');
is($events[2]->{current}->{id}, $old_ws->{id}, 'the "current" property should contain the emptied workspace con');

done_testing;
