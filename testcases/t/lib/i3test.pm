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
    my $original_rect = X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30);

    my $window = X11::XCB::Window->new(
        class => WINDOW_CLASS_INPUT_OUTPUT,
        rect => $original_rect,
        background_color => '#C0C0C0',
    );

    $window->create;
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

1
