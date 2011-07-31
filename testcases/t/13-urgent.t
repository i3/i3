#!perl
# vim:ts=4:sw=4:expandtab

use i3test;
use X11::XCB qw(:all);
use List::Util qw(first);

BEGIN {
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

my $x = X11::XCB::Connection->new;

my $tmp = fresh_workspace;

#####################################################################
# Create two windows and put them in stacking mode
#####################################################################

cmd 'split v';

my $top = open_standard_window($x);
my $bottom = open_standard_window($x);

my @urgent = grep { $_->{urgent} } @{get_ws_content($tmp)};
is(@urgent, 0, 'no window got the urgent flag');

# cmd 'layout stacking';

#####################################################################
# Add the urgency hint, switch to a different workspace and back again
#####################################################################
$top->add_hint('urgency');
sleep 0.5;

@content = @{get_ws_content($tmp)};
@urgent = grep { $_->{urgent} } @content;
$top_info = first { $_->{window} == $top->id } @content;
$bottom_info = first { $_->{window} == $bottom->id } @content;

ok($top_info->{urgent}, 'top window is marked urgent');
ok(!$bottom_info->{urgent}, 'bottom window is not marked urgent');
is(@urgent, 1, 'exactly one window got the urgent flag');

cmd '[id="' . $top->id . '"] focus';

@urgent = grep { $_->{urgent} } @{get_ws_content($tmp)};
is(@urgent, 0, 'no window got the urgent flag after focusing');

$top->add_hint('urgency');
sleep 0.5;

@urgent = grep { $_->{urgent} } @{get_ws_content($tmp)};
is(@urgent, 0, 'no window got the urgent flag after re-setting urgency hint');

#####################################################################
# Check if the workspace urgency hint gets set/cleared correctly
#####################################################################
my $ws = get_ws($tmp);
ok(!$ws->{urgent}, 'urgent flag not set on workspace');

my $otmp = fresh_workspace;

$top->add_hint('urgency');
sleep 0.5;

$ws = get_ws($tmp);
ok($ws->{urgent}, 'urgent flag set on workspace');

cmd "workspace $tmp";

$ws = get_ws($tmp);
ok(!$ws->{urgent}, 'urgent flag not set on workspace after switching');

done_testing;
