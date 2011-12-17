#!perl
# vim:ts=4:sw=4:expandtab
#
# checks if the IPC message type get_marks works correctly
#
use i3test;

sub get_marks {
    return i3(get_socket_path())->get_marks->recv;
}

##############################################################
# 1: check that get_marks returns no marks yet
##############################################################

my $tmp = fresh_workspace;

my $marks = get_marks();
cmp_deeply($marks, [], 'no marks set so far');

##############################################################
# 2: check that setting a mark is reflected in the get_marks reply
##############################################################

cmd 'open';
cmd 'mark foo';

cmp_deeply(get_marks(), [ 'foo' ], 'mark foo set');

##############################################################
# 3: check that the mark is gone after killing the container
##############################################################

cmd 'kill';

cmp_deeply(get_marks(), [ ], 'mark gone');

done_testing;
