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
# Ticket: #1873
use i3test;
use X11::XCB qw(:all);

sub get_wm_state {
    sync_with_i3;

    my ($con) = @_; 
    my $cookie = $x->get_property(
        0,  
        $con->{id},
        $x->atom(name => '_NET_WM_STATE')->id,
        GET_PROPERTY_TYPE_ANY,
        0,  
        4096
    );  

    my $reply = $x->get_property_reply($cookie->{sequence});
    my $len = $reply->{length};
    return 0 if $len == 0;

    my @atoms = unpack("L$len", $reply->{value});
    return \@atoms;
}

my $wm_state_sticky = $x->atom(name => '_NET_WM_STATE_STICKY')->id;
my $wm_state_fullscreen = $x->atom(name => '_NET_WM_STATE_FULLSCREEN')->id;

##########################################################################
# Given a sticky container, when it is fullscreened, then both wm state
# atoms are set. When the container is unfullscreened, then only the
# sticky atom is still set.
##########################################################################

fresh_workspace;
my $window = open_window;
cmd 'sticky enable';
is_deeply(get_wm_state($window), [ $wm_state_sticky ], 'sanity check: _NET_WM_STATE_STICKY is set');

cmd 'fullscreen enable';
is_deeply(get_wm_state($window), [ $wm_state_sticky, $wm_state_fullscreen ],
    'both _NET_WM_STATE_FULLSCREEN and _NET_WM_STATE_STICKY are set');

cmd 'sticky disable';
is_deeply(get_wm_state($window), [ $wm_state_fullscreen ], 'only _NET_WM_STATE_FULLSCREEN is set');

cmd 'sticky enable';
cmd 'fullscreen disable';
is_deeply(get_wm_state($window), [ $wm_state_sticky ], 'only _NET_WM_STATE_STICKY is set');

##########################################################################

done_testing;
