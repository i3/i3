#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)

use i3test i3_autostart => 0;
use List::Util qw(first);

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

force_display_urgency_hint 0ms
EOT
my $pid = launch_with_config($config);

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
sync_with_i3;

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
sync_with_i3;

@urgent = grep { $_->{urgent} } @{get_ws_content($tmp)};
is(@urgent, 0, 'no window got the urgent flag after re-setting urgency hint');

#####################################################################
# Check if the workspace urgency hint gets set/cleared correctly
#####################################################################

my $ws = get_ws($tmp);
ok(!$ws->{urgent}, 'urgent flag not set on workspace');

my $otmp = fresh_workspace;

$top->add_hint('urgency');
sync_with_i3;

$ws = get_ws($tmp);
ok($ws->{urgent}, 'urgent flag set on workspace');

cmd "workspace $tmp";

$ws = get_ws($tmp);
ok(!$ws->{urgent}, 'urgent flag not set on workspace after switching');

################################################################################
# Use the 'urgent' criteria to switch to windows which have the urgency hint set.
################################################################################

# Go to a new workspace, open a different window, verify focus is on it.
$otmp = fresh_workspace;
my $different_window = open_window;
is($x->input_focus, $different_window->id, 'new window focused');

# Add the urgency hint on the other window.
$top->add_hint('urgency');
sync_with_i3;

# Now try to switch to that window and see if focus changes.
cmd '[urgent=latest] focus';
isnt($x->input_focus, $different_window->id, 'window no longer focused');
is($x->input_focus, $top->id, 'urgent window focused');

################################################################################
# Same thing, but with multiple windows and using the 'urgency=latest' criteria
# (verify that it works in the correct order).
################################################################################

cmd "workspace $otmp";
is($x->input_focus, $different_window->id, 'new window focused again');

$top->add_hint('urgency');
sync_with_i3;

$bottom->add_hint('urgency');
sync_with_i3;

cmd '[urgent=latest] focus';
is($x->input_focus, $bottom->id, 'latest urgent window focused');
$bottom->delete_hint('urgency');
sync_with_i3;

cmd '[urgent=latest] focus';
is($x->input_focus, $top->id, 'second urgent window focused');
$top->delete_hint('urgency');
sync_with_i3;

################################################################################
# Same thing, but with multiple windows and using the 'urgency=oldest' criteria
# (verify that it works in the correct order).
################################################################################

cmd "workspace $otmp";
is($x->input_focus, $different_window->id, 'new window focused again');

$top->add_hint('urgency');
sync_with_i3;

$bottom->add_hint('urgency');
sync_with_i3;

cmd '[urgent=oldest] focus';
is($x->input_focus, $top->id, 'oldest urgent window focused');
$top->delete_hint('urgency');
sync_with_i3;

cmd '[urgent=oldest] focus';
is($x->input_focus, $bottom->id, 'oldest urgent window focused');
$bottom->delete_hint('urgency');
sync_with_i3;

exit_gracefully($pid);

done_testing;
