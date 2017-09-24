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
# Tests that _NET_WM_VISIBLE_NAME is set correctly.
# Ticket: #1872
use i3test;
use X11::XCB qw(:all);

my ($con);

sub get_visible_name {
    sync_with_i3;
    my ($con) = @_;

    my $cookie = $x->get_property(
        0,
        $con->{id},
        $x->atom(name => '_NET_WM_VISIBLE_NAME')->id,
        $x->atom(name => 'UTF8_STRING')->id,
        0,
        4096
    );

    my $reply = $x->get_property_reply($cookie->{sequence});
    return undef if $reply->{value_len} == 0;
    return $reply->{value};
}

###############################################################################
# 1: _NET_WM_VISIBLE_NAME is set when the title format of a window is changed.
###############################################################################

fresh_workspace;
$con = open_window(name => 'boring title');
is(get_visible_name($con), undef, 'sanity check: initially no visible name is set');

cmd 'title_format custom';
is(get_visible_name($con), 'custom', 'the visible name is updated');

cmd 'title_format "<s>%title</s>"';
is(get_visible_name($con), '<s>boring title</s>', 'markup is returned as is');

###############################################################################
# 2: _NET_WM_VISIBLE_NAME is removed if not needed.
###############################################################################

fresh_workspace;
$con = open_window(name => 'boring title');
cmd 'title_format custom';
is(get_visible_name($con), 'custom', 'sanity check: a visible name is set');

cmd 'title_format %title';
is(get_visible_name($con), undef, 'the visible name is removed again');

###############################################################################

done_testing;
