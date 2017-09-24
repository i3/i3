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
# Test for _NET_WM_USER_TIME.
# Ticket: #2064
use i3test;
use X11::XCB 'PROP_MODE_REPLACE';

my ($ws, $other, $con);

sub open_window_with_user_time {
    my $wm_user_time = shift;

    my $window = open_window(
        before_map => sub {
            my ($window) = @_;

            my $atomname = $x->atom(name => '_NET_WM_USER_TIME');
            my $atomtype = $x->atom(name => 'CARDINAL');
            $x->change_property(
                PROP_MODE_REPLACE,
                $window->id,
                $atomname->id,
                $atomtype->id,
                32,
                1,
                pack('L1', $wm_user_time),
            );
        },
    );

    return $window;
}

#####################################################################
# 1: if _NET_WM_USER_TIME is set to 0, the window is not focused
#    initially.
#####################################################################

$ws = fresh_workspace;

open_window;
$other = get_focused($ws);
open_window_with_user_time(0);

is(get_focused($ws), $other, 'new window is not focused');

#####################################################################
# 2: if _NET_WM_USER_TIME is set to something other than 0, the
#    window is focused anyway.
#####################################################################

$ws = fresh_workspace;

open_window;
$other = get_focused($ws);
open_window_with_user_time(42);

isnt(get_focused($ws), $other, 'new window is focused');

done_testing;
