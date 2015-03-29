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
#
#
# Tests whether the urgency timer works as expected and does not break
# urgency handling.
#

use List::Util qw(first);
use i3test i3_autostart => 0;
use Time::HiRes qw(sleep);

# Ensure the pointer is at (0, 0) so that we really start on the first
# (the left) workspace.
$x->root->warp_pointer(0, 0);

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

force_display_urgency_hint 500ms
EOT
my $pid = launch_with_config($config);

#####################################################################
# Initial setup: one window on ws1, empty ws2
#####################################################################

my $tmp1 = fresh_workspace;
my $w = open_window;

my $tmp2 = fresh_workspace;
cmd "workspace $tmp2";

$w->add_hint('urgency');
sync_with_i3;

#######################################################################
# Create a window on ws1, then switch to ws2, set urgency, switch back
#######################################################################

isnt($x->input_focus, $w->id, 'window not focused');

my @content = @{get_ws_content($tmp1)};
my @urgent = grep { $_->{urgent} } @content;
is(@urgent, 1, "window marked as urgent");

# switch to ws1
cmd "workspace $tmp1";

# this will start the timer
sleep(0.1);
@content = @{get_ws_content($tmp1)};
@urgent = grep { $_->{urgent} } @content;
is(@urgent, 1, 'window still marked as urgent');

# now check if the timer was triggered
cmd "workspace $tmp2";

sleep(0.5);
@content = @{get_ws_content($tmp1)};
@urgent = grep { $_->{urgent} } @content;
is(@urgent, 0, 'window not marked as urgent anymore');

#######################################################################
# Create another window on ws1, focus it, switch to ws2, make the other
# window urgent, and switch back. This should not trigger the timer.
#######################################################################

cmd "workspace $tmp1";
my $w2 = open_window;
is($x->input_focus, $w2->id, 'window 2 focused');

cmd "workspace $tmp2";
$w->delete_hint('urgency');
$w->add_hint('urgency');
sync_with_i3;

@content = @{get_ws_content($tmp1)};
@urgent = grep { $_->{urgent} } @content;
is(@urgent, 1, 'window 1 marked as urgent');

# Switch back to ws1. This should focus w2.
cmd "workspace $tmp1";
is($x->input_focus, $w2->id, 'window 2 focused');

@content = @{get_ws_content($tmp1)};
@urgent = grep { $_->{urgent} } @content;
is(@urgent, 1, 'window 1 still marked as urgent');

# explicitly focusing the window should result in immediate urgency reset
cmd '[id="' . $w->id . '"] focus';
@content = @{get_ws_content($tmp1)};
@urgent = grep { $_->{urgent} } @content;
is(@urgent, 0, 'window 1 not marked as urgent anymore');

################################################################################
# open a stack, mark one window as urgent, switch to that workspace and verify
# it’s cleared correctly.
################################################################################

sub count_total_urgent {
    my ($con) = @_;

    my $urgent = ($con->{urgent} ? 1 : 0);
    $urgent += count_total_urgent($_) for (@{$con->{nodes}}, @{$con->{floating_nodes}});
    return $urgent;
}

my $tmp3 = fresh_workspace;
open_window;
open_window;
cmd 'split v';
my $split_left = open_window;
cmd 'layout stacked';

cmd "workspace $tmp2";

is(count_total_urgent(get_ws($tmp3)), 0, "no urgent windows on workspace $tmp3");

$split_left->add_hint('urgency');
sync_with_i3;

cmp_ok(count_total_urgent(get_ws($tmp3)), '>=', 0, "more than one urgent window on workspace $tmp3");

cmd "workspace $tmp3";

# Remove the urgency hint.
$split_left->delete_hint('urgency');
sync_with_i3;

sleep(0.6);
is(count_total_urgent(get_ws($tmp3)), 0, "no more urgent windows on workspace $tmp3");

exit_gracefully($pid);

done_testing;
