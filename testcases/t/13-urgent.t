#!perl
# vim:ts=4:sw=4:expandtab

use i3test tests => 10;
use X11::XCB qw(:all);
use Time::HiRes qw(sleep);
use List::Util qw(first);

BEGIN {
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

my $x = X11::XCB::Connection->new;

my $i3 = i3("/tmp/nestedcons");
my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

#####################################################################
# Create two windows and put them in stacking mode
#####################################################################

$i3->command('split v')->recv;

my $top = i3test::open_standard_window($x);
my $bottom = i3test::open_standard_window($x);

my @urgent = grep { $_->{urgent} == 1 } @{get_ws_content($tmp)};
is(@urgent, 0, 'no window got the urgent flag');

#$i3->command('layout stacking')->recv;

#####################################################################
# Add the urgency hint, switch to a different workspace and back again
#####################################################################
$top->add_hint('urgency');
sleep 0.5;

@content = @{get_ws_content($tmp)};
@urgent = grep { $_->{urgent} == 1 } @content;
$top_info = first { $_->{window} == $top->id } @content;
$bottom_info = first { $_->{window} == $bottom->id } @content;

is($top_info->{urgent}, 1, 'top window is marked urgent');
is($bottom_info->{urgent}, 0, 'bottom window is not marked urgent');
is(@urgent, 1, 'exactly one window got the urgent flag');

$i3->command('[id="' . $top->id . '"] focus')->recv;

@urgent = grep { $_->{urgent} == 1 } @{get_ws_content($tmp)};
is(@urgent, 0, 'no window got the urgent flag after focusing');

$top->add_hint('urgency');
sleep 0.5;

@urgent = grep { $_->{urgent} == 1 } @{get_ws_content($tmp)};
is(@urgent, 0, 'no window got the urgent flag after re-setting urgency hint');

#####################################################################
# Check if the workspace urgency hint gets set/cleared correctly
#####################################################################
my $ws = get_ws($tmp);
is($ws->{urgent}, 0, 'urgent flag not set on workspace');

my $otmp = get_unused_workspace();
$i3->command("workspace $otmp")->recv;

$top->add_hint('urgency');
sleep 0.5;

$ws = get_ws($tmp);
is($ws->{urgent}, 1, 'urgent flag set on workspace');

$i3->command("workspace $tmp")->recv;

$ws = get_ws($tmp);
is($ws->{urgent}, 0, 'urgent flag not set on workspace after switching');

diag( "Testing i3, Perl $], $^X" );
