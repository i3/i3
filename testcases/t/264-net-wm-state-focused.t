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
# Tests for setting and removing the _NET_WM_STATE_FOCUSED atom properly.
# Ticket: #2273
use i3test;
use X11::XCB qw(:all);

sub is_focused {
    sync_with_i3;
    my $atom = $x->atom(name => '_NET_WM_STATE_FOCUSED');

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
    for (my $i = 0; $i < $len; $i++) {
        return 1 if $atoms[$i] == $atom->id;
    }

    return 0;
}

my ($windowA, $windowB);

fresh_workspace;
$windowA = open_window;

ok(is_focused($windowA), 'a newly opened window that is focused should have _NET_WM_STATE_FOCUSED set');

$windowB = open_window;

ok(!is_focused($windowA), 'when a another window is focused, the old window should not have _NET_WM_STATE_FOCUSED set');

fresh_workspace;

ok(!is_focused($windowB), 'when focus moves to the ewmh support window, neither window should have _NET_WM_STATE_FOCUSED set');

# TODO: test to make sure the atom is properly set/unset when WM_TAKE_FOCUS is used to focus the window

done_testing;
