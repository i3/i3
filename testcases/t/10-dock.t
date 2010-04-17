#!perl
# vim:ts=4:sw=4:expandtab

use i3test tests => 2;
use X11::XCB qw(:all);
use Time::HiRes qw(sleep);
use List::Util qw(first);

BEGIN {
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

my $x = X11::XCB::Connection->new;

#####################################################################
# Create a dock window and see if it gets managed
#####################################################################

my $screens = $x->screens;

# Get the primary screen
my $primary = first { $_->primary } @{$screens};

# TODO: focus the primary screen before

my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#FF0000',
    type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
);

$window->map;

sleep 0.25;

my $rect = $window->rect;
is($rect->width, $primary->rect->width, 'dock client is as wide as the screen');

my $fwindow = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#FF0000',
    type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
);

$fwindow->transient_for($window);
$fwindow->map;

sleep 0.25;


diag( "Testing i3, Perl $], $^X" );
