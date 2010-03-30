#!perl
# vim:ts=4:sw=4:expandtab
# Beware that this test uses workspace 9 and 10 to perform some tests (it expects
# the workspace to be empty).
# TODO: skip it by default?

use Test::More tests => 3;
use Test::Deep;
use X11::XCB qw(:all);
use Data::Dumper;
use Time::HiRes qw(sleep);
use FindBin;
use Digest::SHA1 qw(sha1_base64);
use lib "$FindBin::Bin/lib";
use i3test;
use AnyEvent::I3;

BEGIN {
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

my $x = X11::XCB::Connection->new;
my $i3 = i3;

# Switch to the nineth workspace
$i3->command('9')->recv;

#####################################################################
# Create a parent window
#####################################################################

my $window = $x->root->create_child(
class => WINDOW_CLASS_INPUT_OUTPUT,
rect => [ 0, 0, 30, 30 ],
background_color => '#C0C0C0',
);

$window->name('Parent window');
$window->map;

sleep 0.25;

#########################################################################
# Switch workspace to 10 and open a child window. It should be positioned
# on workspace 9.
#########################################################################
$i3->command('10')->recv;

my $child = $x->root->create_child(
class => WINDOW_CLASS_INPUT_OUTPUT,
rect => [ 0, 0, 30, 30 ],
background_color => '#C0C0C0',
);

$child->name('Child window');
$child->client_leader($window);
$child->map;

sleep 0.25;

isnt($x->input_focus, $child->id, "Child window focused");

# Switch back
$i3->command('9')->recv;

is($x->input_focus, $child->id, "Child window focused");
