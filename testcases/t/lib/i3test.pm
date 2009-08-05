package i3test;
# vim:ts=4:sw=4:expandtab

use X11::XCB::Rect;
use X11::XCB::Window;
use X11::XCB qw(:all);

sub open_standard_window {
    my $original_rect = X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30);

    my $window = X11::XCB::Window->new(
        class => WINDOW_CLASS_INPUT_OUTPUT,
        rect => $original_rect,
        background_color => 12632256,
    );

    $window->create;
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

1
