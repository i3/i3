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
# Verifies that windows with properties that indicate they should be floating
# are indeed opened floating.
# Ticket: #1182
# Bug still in: 4.7.2-97-g84fc808
use i3test;
use X11::XCB qw(PROP_MODE_REPLACE);

sub open_with_type {
    my $window_type = shift;

    my $window = open_window(
        window_type => $x->atom(name => $window_type),
    );
    return $window;
}

sub open_with_state {
    my $window_state = shift;

    my $window = open_window(
        before_map => sub {
            my ($window) = @_;

            my $atomname = $x->atom(name => '_NET_WM_STATE');
            my $atomtype = $x->atom(name => 'ATOM');
            $x->change_property(
                PROP_MODE_REPLACE,
                $window->id,
                $atomname->id,
                $atomtype->id,
                32,
                1,
                pack('L1', $x->atom(name => $window_state)->id),
            );
        },
    );

    return $window;
}

sub open_with_fixed_size {
    # The type of the WM_NORMAL_HINTS property is WM_SIZE_HINTS
    # https://tronche.com/gui/x/icccm/sec-4.html#s-4.1.2.3
    my $XCB_ICCCM_SIZE_HINT_P_MIN_SIZE = 0x32;
    my $XCB_ICCCM_SIZE_HINT_P_MAX_SIZE = 0x16;

    my $flags = $XCB_ICCCM_SIZE_HINT_P_MIN_SIZE | $XCB_ICCCM_SIZE_HINT_P_MAX_SIZE;

    my $min_width = 150;
    my $max_width = 150;
    my $min_height = 100;
    my $max_height = 100;

    my $pad = 0x00;

    my $window = open_window(
        before_map => sub {
            my ($window) = @_;

            my $atomname = $x->atom(name => 'WM_NORMAL_HINTS');
            my $atomtype = $x->atom(name => 'WM_SIZE_HINTS');
            $x->change_property(
                PROP_MODE_REPLACE,
                $window->id,
                $atomname->id,
                $atomtype->id,
                32,
                13,
                pack('C5N8', $flags, $pad, $pad, $pad, $pad, 0, 0, 0, $min_width, $min_height, $max_width, $max_height),
            );
        },
    );

    return $window;
}

my $ws = fresh_workspace;

my $window = open_with_type '_NET_WM_WINDOW_TYPE_DIALOG';
is(get_ws($ws)->{floating_nodes}[0]->{nodes}[0]->{window}, $window->id, 'Dialog window opened floating');
$window->unmap;

$window = open_with_type '_NET_WM_WINDOW_TYPE_UTILITY';
is(get_ws($ws)->{floating_nodes}[0]->{nodes}[0]->{window}, $window->id, 'Utility window opened floating');
$window->unmap;

$window = open_with_type '_NET_WM_WINDOW_TYPE_TOOLBAR';
is(get_ws($ws)->{floating_nodes}[0]->{nodes}[0]->{window}, $window->id, 'Toolbar window opened floating');
$window->unmap;

$window = open_with_type '_NET_WM_WINDOW_TYPE_SPLASH';
is(get_ws($ws)->{floating_nodes}[0]->{nodes}[0]->{window}, $window->id, 'Splash window opened floating');
$window->unmap;

$window = open_with_state '_NET_WM_STATE_MODAL';
is(get_ws($ws)->{floating_nodes}[0]->{nodes}[0]->{window}, $window->id, 'Modal window opened floating');
$window->unmap;

$window = open_with_fixed_size;
is(get_ws($ws)->{floating_nodes}[0]->{nodes}[0]->{window}, $window->id, 'Fixed size window opened floating');
is(get_ws($ws)->{floating_nodes}[0]->{nodes}[0]->{window_rect}->{width}, 150, 'Fixed size window opened with minimum width');
is(get_ws($ws)->{floating_nodes}[0]->{nodes}[0]->{window_rect}->{height}, 100, 'Fixed size window opened with minimum height');
$window->unmap;

done_testing;
