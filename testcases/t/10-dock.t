#!perl
# vim:ts=4:sw=4:expandtab

use Test::More tests => 2;
use Test::Deep;
use X11::XCB qw(:all);
use Data::Dumper;
use Time::HiRes qw(sleep);
use FindBin;
use lib "$FindBin::Bin/lib";
use i3test;
use List::Util qw(first);

BEGIN {
    #use_ok('IO::Socket::UNIX') or BAIL_OUT('Cannot load IO::Socket::UNIX');
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

X11::XCB::Connection->connect(':0');

#####################################################################
# Create a dock window and see if it gets managed
#####################################################################

my $screens = X11::XCB::Connection->screens;

# Get the primary screen
my $primary = first { $_->primary } @{$screens};

# TODO: focus the primary screen before

my $window = X11::XCB::Window->new(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    background_color => '#FF0000',
    type => 'dock',
);

$window->create;
$window->map;

sleep 0.25;

my $rect = $window->rect;
is($rect->width, $primary->rect->width, 'dock client is as wide as the screen');

diag( "Testing i3, Perl $], $^X" );
