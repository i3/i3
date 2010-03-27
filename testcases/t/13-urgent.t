#!perl
# vim:ts=4:sw=4:expandtab
# Beware that this test uses workspace 9 to perform some tests (it expects
# the workspace to be empty).
# TODO: skip it by default?

use Test::More tests => 7;
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
# Create two windows and put them in stacking mode
#####################################################################

my $top = i3test::open_standard_window($x);
sleep 0.25;
my $bottom = i3test::open_standard_window($x);
sleep 0.25;

$i3->command('s')->recv;

#####################################################################
# Add the urgency hint, switch to a different workspace and back again
#####################################################################
$top->add_hint('urgency');
sleep 1;

$i3->command('1')->recv;
$i3->command('9')->recv;
$i3->command('1')->recv;

my $std = i3test::open_standard_window($x);
sleep 0.25;
$std->add_hint('urgency');
sleep 1;
