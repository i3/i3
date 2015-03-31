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
# checks if mark and unmark work correctly
use i3test;
use List::Util qw(first);

sub get_marks {
    return i3(get_socket_path())->get_marks->recv;
}

sub get_mark_for_window_on_workspace {
    my ($ws, $con) = @_;

    my $current = first { $_->{window} == $con->{id} } @{get_ws_content($ws)};
    return $current->{mark};
}

##############################################################
# 1: check that there are no marks set yet
##############################################################

my $tmp = fresh_workspace;

cmd 'split h';

is_deeply(get_marks(), [], 'no marks set yet');


##############################################################
# 2: mark a con, check that it's marked, unmark it, check that
##############################################################

my $one = open_window;
cmd 'mark foo';

is_deeply(get_marks(), ["foo"], 'mark foo set');

cmd 'unmark foo';

is_deeply(get_marks(), [], 'mark foo removed');

##############################################################
# 3: mark three cons, check that they are marked
#    unmark one con, check that it's unmarked
#    unmark all cons, check that they are unmarked
##############################################################

my $left = open_window;
my $middle = open_window;
my $right = open_window;

cmd 'mark right';
cmd 'focus left';
cmd 'mark middle';
cmd 'focus left';
cmd 'mark left';

#
# get_marks replys an array of marks, whose order is undefined,
# so we use sort to be able to compare the output
#

is_deeply(sort(get_marks()), ["left","middle","right"], 'all three marks set');

cmd 'unmark right';

is_deeply(sort(get_marks()), ["left","middle"], 'mark right removed');

cmd 'unmark';

is_deeply(get_marks(), [], 'all marks removed');

##############################################################
# 4: mark a con, use same mark to mark another con,
#    check that only the latter is marked
##############################################################

my $first = open_window;
my $second = open_window;

cmd 'mark important';
cmd 'focus left';
cmd 'mark important';

is(get_mark_for_window_on_workspace($tmp, $first), 'important', 'first container now has the mark');
ok(!get_mark_for_window_on_workspace($tmp, $second), 'second container lost the mark');

##############################################################
# 5: mark a con, toggle the mark, check that the mark is gone
##############################################################

my $con = open_window;
cmd 'mark important';
cmd 'mark --toggle important';
ok(!get_mark_for_window_on_workspace($tmp, $con), 'container no longer has the mark');

##############################################################
# 6: toggle a mark on an unmarked con, check it is marked
##############################################################

my $con = open_window;
cmd 'mark --toggle important';
is(get_mark_for_window_on_workspace($tmp, $con), 'important', 'container now has the mark');

##############################################################
# 7: mark a con, toggle a different mark, check it is marked
#    with the new mark
##############################################################

my $con = open_window;
cmd 'mark boring';
cmd 'mark --toggle important';
is(get_mark_for_window_on_workspace($tmp, $con), 'important', 'container has the most recent mark');

##############################################################
# 8: mark a con, toggle the mark on another con,
#    check only the latter has the mark
##############################################################

my $first = open_window;
my $second = open_window;

cmd 'mark important';
cmd 'focus left';
cmd 'mark --toggle important';

is(get_mark_for_window_on_workspace($tmp, $first), 'important', 'left container has the mark now');
ok(!get_mark_for_window_on_workspace($tmp, $second), 'second containr no longer has the mark');

##############################################################
# 9: try to mark two cons with the same mark and check that
#    it fails
##############################################################

my $first = open_window(wm_class => 'iamnotunique');
my $second = open_window(wm_class => 'iamnotunique');

my $result = cmd "[instance=iamnotunique] mark important";

is($result->[0]->{success}, 0, 'command was unsuccessful');
is($result->[0]->{error}, 'A mark must not be put onto more than one window', 'correct error is returned');
ok(!get_mark_for_window_on_workspace($tmp, $first), 'first container is not marked');
ok(!get_mark_for_window_on_workspace($tmp, $second), 'second containr is not marked');

##############################################################

done_testing;
