#!perl
# vim:ts=4:sw=4:expandtab
#
# checks if the IPC message type get_marks works correctly
#
use i3test;

# TODO: this will be available in AnyEvent::I3 soon
sub get_marks {
    my $i3 = i3(get_socket_path());
    $i3->connect->recv;
    my $cv = AnyEvent->condvar;
    my $msg = $i3->message(5);
    my $t;
    $msg->cb(sub {
        my ($_cv) = @_;
        $cv->send($_cv->recv);
    });
    $t = AnyEvent->timer(after => 2, cb => sub {
        $cv->croak('timeout while waiting for the marks');
    });
    return $cv->recv;
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

##############################################################
# 4: check that duplicate marks are included twice in the get_marks reply
##############################################################

cmd 'open';
cmd 'mark bar';

cmd 'open';
cmd 'mark bar';

cmp_deeply(get_marks(), [ 'bar', 'bar' ], 'duplicate mark found twice');

done_testing;
