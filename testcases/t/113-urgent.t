#!perl
# vim:ts=4:sw=4:expandtab

use i3test;
use List::Util qw(first);

my $tmp = fresh_workspace;

#####################################################################
# Create two windows and put them in stacking mode
#####################################################################

cmd 'split v';

my $top = open_window;
my $bottom = open_window;

my @urgent = grep { $_->{urgent} } @{get_ws_content($tmp)};
is(@urgent, 0, 'no window got the urgent flag');

# cmd 'layout stacking';

#####################################################################
# Add the urgency hint, switch to a different workspace and back again
#####################################################################
$top->add_hint('urgency');
sync_with_i3($x);

my @content = @{get_ws_content($tmp)};
@urgent = grep { $_->{urgent} } @content;
my $top_info = first { $_->{window} == $top->id } @content;
my $bottom_info = first { $_->{window} == $bottom->id } @content;

ok($top_info->{urgent}, 'top window is marked urgent');
ok(!$bottom_info->{urgent}, 'bottom window is not marked urgent');
is(@urgent, 1, 'exactly one window got the urgent flag');

cmd '[id="' . $top->id . '"] focus';

@urgent = grep { $_->{urgent} } @{get_ws_content($tmp)};
is(@urgent, 0, 'no window got the urgent flag after focusing');

$top->add_hint('urgency');
sync_with_i3($x);

@urgent = grep { $_->{urgent} } @{get_ws_content($tmp)};
is(@urgent, 0, 'no window got the urgent flag after re-setting urgency hint');

#####################################################################
# Check if the workspace urgency hint gets set/cleared correctly
#####################################################################
my $ws = get_ws($tmp);
ok(!$ws->{urgent}, 'urgent flag not set on workspace');

my $otmp = fresh_workspace;

$top->add_hint('urgency');
sync_with_i3($x);

$ws = get_ws($tmp);
ok($ws->{urgent}, 'urgent flag set on workspace');

cmd "workspace $tmp";

$ws = get_ws($tmp);
ok(!$ws->{urgent}, 'urgent flag not set on workspace after switching');

done_testing;
