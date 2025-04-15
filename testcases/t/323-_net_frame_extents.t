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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Verify _NET_FRAME_EXTENTS is set correctly
# Ticket: #4292
# Bug still in: 4.24-11-g11927eb0
use i3test;
use X11::XCB qw(:all);
use Data::Dumper;

sub net_frame_extends {
    my ($window) = @_;

    my $cookie = $x->get_property(
        0,
        $window->{id},
        $x->atom(name => '_NET_FRAME_EXTENTS')->id,
        GET_PROPERTY_TYPE_ANY,
        0,
        4
    );

    my $reply = $x->get_property_reply($cookie->{sequence});
    my $len = $reply->{length};
    return [] if $len == 0;

    return unpack("L$len", $reply->{value});
}

sub is_net_frame_extends {
    my ($window, $expect) = @_;
    my @extends = net_frame_extends($window);
    is_deeply(\@extends, $expect, "window extends @$expect");
}

my $w = open_window;

cmd 'border normal 3';
is_net_frame_extends($w, [3, 3, 18, 3]);

cmd 'border pixel 1';
is_net_frame_extends($w, [1, 1, 1, 1]);

cmd 'border pixel 5';
is_net_frame_extends($w, [5, 5, 5, 5]);

# TODO: stack, tabbed, many windows, hide edge borders

done_testing;
