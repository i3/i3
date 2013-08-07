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

sub get_marks {
    return i3(get_socket_path())->get_marks->recv;
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

done_testing;
