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
# Tests for mark/unmark with multiple marks on a single window.
# Ticket: #2014
use i3test;
use List::Util qw(first);

my ($ws, $con, $first, $second);

sub get_marks {
    return i3(get_socket_path())->get_marks->recv;
}

sub get_mark_for_window_on_workspace {
    my ($ws, $con) = @_;

    my $current = first { $_->{window} == $con->{id} } @{get_ws_content($ws)};
    return $current->{marks};
}

###############################################################################
# Verify that multiple marks can be set on a window.
###############################################################################

$ws = fresh_workspace;
$con = open_window;
cmd 'mark --add A';
cmd 'mark --add B';

is_deeply(sort(get_marks()), [ 'A', 'B' ], 'both marks exist');
is_deeply(get_mark_for_window_on_workspace($ws, $con), [ 'A', 'B' ], 'both marks are on the same window');

cmd 'unmark';

###############################################################################
# Verify that toggling a mark can affect only the specified mark.
###############################################################################

$ws = fresh_workspace;
$con = open_window;
cmd 'mark A';

cmd 'mark --add --toggle B';
is_deeply(get_mark_for_window_on_workspace($ws, $con), [ 'A', 'B' ], 'both marks are on the same window');
cmd 'mark --add --toggle B';
is_deeply(get_mark_for_window_on_workspace($ws, $con), [ 'A' ], 'only mark B has been removed');

cmd 'unmark';

###############################################################################
# Verify that unmarking a mark leaves other marks on the same window intact.
###############################################################################

$ws = fresh_workspace;
$con = open_window;
cmd 'mark --add A';
cmd 'mark --add B';
cmd 'mark --add C';

cmd 'unmark B';
is_deeply(get_mark_for_window_on_workspace($ws, $con), [ 'A', 'C' ], 'only mark B has been removed');

cmd 'unmark';

###############################################################################
# Verify that matching via mark works on windows with multiple marks.
###############################################################################

$ws = fresh_workspace;
$con = open_window;
cmd 'mark --add A';
cmd 'mark --add B';
open_window;

cmd '[con_mark=B] mark --add C';
is_deeply(get_mark_for_window_on_workspace($ws, $con), [ 'A', 'B', 'C' ], 'matching on a mark works with multiple marks');

cmd 'unmark';

###############################################################################
# Verify that "unmark" can be matched on the focused window.
###############################################################################

$ws = fresh_workspace;
$con = open_window;
cmd 'mark --add A';
cmd 'mark --add B';
open_window;
cmd 'mark --add C';
cmd 'mark --add D';

is_deeply(sort(get_marks()), [ 'A', 'B', 'C', 'D' ], 'all marks exist');

cmd '[con_id=__focused__] unmark';

is_deeply(sort(get_marks()), [ 'A', 'B' ], 'marks on the unfocused window still exist');
is_deeply(get_mark_for_window_on_workspace($ws, $con), [ 'A', 'B' ], 'matching on con_id=__focused__ works for unmark');

cmd 'unmark';

###############################################################################

done_testing;
