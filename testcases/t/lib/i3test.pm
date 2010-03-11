package i3test;
# vim:ts=4:sw=4:expandtab

use X11::XCB::Rect;
use X11::XCB::Window;
use X11::XCB qw(:all);

BEGIN {
    my $window_count = 0;
    sub counter_window {
        return $window_count++;
    }
}

sub open_standard_window {
    my ($x) = @_;

    my $window = $x->root->create_child(
        class => WINDOW_CLASS_INPUT_OUTPUT,
        rect => [ 0, 0, 30, 30 ],
        background_color => '#C0C0C0',
    );

    $window->name('Window ' . counter_window());
    $window->map;

    sleep(0.25);

    return $window;
}

sub format_ipc_command {
    my $msg = shift;
    my $len;

    { use bytes; $len = length($msg); }

    my $message = "i3-ipc" . pack("LL", $len, 0) . $msg;

    return $message;
}

sub recv_ipc_command {
    my ($sock, $expected) = @_;

    my $buffer;
    # header is 14 bytes ("i3-ipc" + 32 bit + 32 bit)
    $sock->read($buffer, 14);
    return undef unless substr($buffer, 0, length("i3-ipc")) eq "i3-ipc";

    my ($len, $type) = unpack("LL", substr($buffer, 6));

    return undef unless $type == $expected;

    # read the payload
    $sock->read($buffer, $len);

    decode_json($buffer)
}

1
