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
# Tests for our proprietary atom I3_FLOATING_WINDOW to allow
# identifying floating windows.
# Ticket: #2223
use i3test;
use X11::XCB qw(:all);

my ($con);

sub has_i3_floating_window {
    sync_with_i3;

    my ($con) = @_;
    my $cookie = $x->get_property(
        0,
        $con->{id},
        $x->atom(name => 'I3_FLOATING_WINDOW')->id,
        $x->atom(name => 'CARDINAL')->id,
        0,
        1
    );

    my $reply = $x->get_property_reply($cookie->{sequence});
    return 0 if $reply->{length} != 1;

    return unpack("L", $reply->{value});
}

###############################################################################
# Toggling floating on a container adds / removes I3_FLOATING_WINDOW.
###############################################################################

fresh_workspace;

$con = open_window;
is(has_i3_floating_window($con), 0, 'I3_FLOATING_WINDOW is not set');

cmd 'floating enable';
is(has_i3_floating_window($con), 1, 'I3_FLOATING_WINDOW is set');

cmd 'floating disable';
is(has_i3_floating_window($con), 0, 'I3_FLOATING_WINDOW is not set');

###############################################################################
# A window that is floated when managed has I3_FLOATING_WINDOW set.
###############################################################################
#
fresh_workspace;

$con = open_floating_window;
is(has_i3_floating_window($con), 1, 'I3_FLOATING_WINDOW is set');

###############################################################################

done_testing;
