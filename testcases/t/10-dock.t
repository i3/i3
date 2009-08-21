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

BEGIN {
    #use_ok('IO::Socket::UNIX') or BAIL_OUT('Cannot load IO::Socket::UNIX');
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

X11::XCB::Connection->connect(':0');

#####################################################################
# Create a dock window and see if it gets managed
#####################################################################

my $window = X11::XCB::Window->new(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 0, 0, 30, 30],
    #override_redirect => 1,
    background_color => 12632256,
    type => 'dock',
);

$window->create;
$window->map;

diag("dimensions before sleep: " . Dumper($window->rect));

sleep 0.25;

# TODO: check if it is as wide as the screen is

diag("dimensions after sleep: " . Dumper($window->rect));
