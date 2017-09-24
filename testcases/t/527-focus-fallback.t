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
# Ticket: #1378
use i3test;
use X11::XCB qw(:all);

sub get_ewmh_window {
    my $cookie = $x->get_property(
        0,
        $x->get_root_window(),
        $x->atom(name => '_NET_SUPPORTING_WM_CHECK')->id,
        $x->atom(name => 'WINDOW')->id,
        0,
        4096
    );

    my $reply = $x->get_property_reply($cookie->{sequence});
    my $len = $reply->{length};
    return undef if $len == 0;

    return unpack("L", $reply->{value});
}

my $window = open_window;
sync_with_i3;
is($x->input_focus, $window->id, 'sanity check: window has input focus');
cmd 'kill';
sync_with_i3;
is($x->input_focus, get_ewmh_window(), 'focus falls back to the EWMH window');

done_testing;
