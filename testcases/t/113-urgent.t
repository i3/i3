#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • https://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • https://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • https://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)

use i3test i3_autostart => 0;
use List::Util qw(first);

my $_NET_WM_STATE_REMOVE = 0;
my $_NET_WM_STATE_ADD = 1;
my $_NET_WM_STATE_TOGGLE = 2;

sub set_urgency {
    my ($win, $urgent_flag, $type) = @_;
    if ($type == 1) {
        # Because X11::XCB does not keep track of clearing the urgency hint
        # when receiving focus, we just delete it in all cases and then re-set
        # it if appropriate.
        $win->delete_hint('urgency');
        $win->add_hint('urgency') if ($urgent_flag);
    } elsif ($type == 2) {
        my $msg = pack "CCSLLLLLL",
            X11::XCB::CLIENT_MESSAGE, # response_type
            32, # format
            0, # sequence
            $win->id, # window
            $x->atom(name => '_NET_WM_STATE')->id, # message type
            ($urgent_flag ? $_NET_WM_STATE_ADD : $_NET_WM_STATE_REMOVE), # data32[0]
            $x->atom(name => '_NET_WM_STATE_DEMANDS_ATTENTION')->id, # data32[1]
            0, # data32[2]
            0, # data32[3]
            0; # data32[4]

        $x->send_event(0, $x->get_root_window(), X11::XCB::EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);
    }
}

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

force_display_urgency_hint 0ms
EOT

my ($type, $tmp, $w1, $w2);
for ($type = 1; $type <= 2; $type++) {
    my $pid = launch_with_config($config);
    $tmp = fresh_workspace;

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
    set_urgency($top, 1, $type);
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

    set_urgency($top, 1, $type);
    sync_with_i3;

    @urgent = grep { $_->{urgent} } @{get_ws_content($tmp)};
    is(@urgent, 0, 'no window got the urgent flag after re-setting urgency hint');

#####################################################################
# Check if the workspace urgency hint gets set/cleared correctly
#####################################################################

    my $ws = get_ws($tmp);
    ok(!$ws->{urgent}, 'urgent flag not set on workspace');

    my $otmp = fresh_workspace;

    set_urgency($top, 1, $type);
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
    set_urgency($top, 1, $type);
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

    set_urgency($top, 1, $type);
    sync_with_i3;

    set_urgency($bottom, 1, $type);
    sync_with_i3;

    cmd '[urgent=latest] focus';
    is($x->input_focus, $bottom->id, 'latest urgent window focused');
    set_urgency($bottom, 0, $type);
    sync_with_i3;

    cmd '[urgent=latest] focus';
    is($x->input_focus, $top->id, 'second urgent window focused');
    set_urgency($top, 0, $type);
    sync_with_i3;

################################################################################
# Same thing, but with multiple windows and using the 'urgency=oldest' criteria
# (verify that it works in the correct order).
################################################################################

    cmd "workspace $otmp";
    is($x->input_focus, $different_window->id, 'new window focused again');

    set_urgency($top, 1, $type);
    sync_with_i3;

    set_urgency($bottom, 1, $type);
    sync_with_i3;

    cmd '[urgent=oldest] focus';
    is($x->input_focus, $top->id, 'oldest urgent window focused');
    set_urgency($top, 0, $type);
    sync_with_i3;

    cmd '[urgent=oldest] focus';
    is($x->input_focus, $bottom->id, 'oldest urgent window focused');
    set_urgency($bottom, 0, $type);
    sync_with_i3;

################################################################################
# Check if urgent flag gets propagated to parent containers
################################################################################

    cmd 'split v';



    sub count_urgent {
        my ($con) = @_;

        my @children = (@{$con->{nodes}}, @{$con->{floating_nodes}});
        my $urgent = grep { $_->{urgent} } @children;
        $urgent += count_urgent($_) for @children;
        return $urgent;
    }

    $tmp = fresh_workspace;

    my $win1 = open_window;
    my $win2 = open_window;
    cmd 'layout stacked';
    cmd 'split vertical';
    my $win3 = open_window;
    my $win4 = open_window;
    cmd 'split horizontal' ;
    my $win5 = open_window;
    my $win6 = open_window;

    sync_with_i3;


    my $urgent = count_urgent(get_ws($tmp));
    is($urgent, 0, 'no window got the urgent flag');

    cmd '[id="' . $win2->id . '"] focus';
    sync_with_i3;
    set_urgency($win5, 1, $type);
    set_urgency($win6, 1, $type);
    sync_with_i3;

# we should have 5 urgent cons. win5, win6 and their 3 split parents.

    $urgent = count_urgent(get_ws($tmp));
    is($urgent, 5, '2 windows and 3 split containers got the urgent flag');

    cmd '[id="' . $win5->id . '"] focus';
    sync_with_i3;

# now win5 and still the split parents should be urgent.
    $urgent = count_urgent(get_ws($tmp));
    is($urgent, 4, '1 window and 3 split containers got the urgent flag');

    cmd '[id="' . $win6->id . '"] focus';
    sync_with_i3;

# now now window should be urgent.
    $urgent = count_urgent(get_ws($tmp));
    is($urgent, 0, 'All urgent flags got cleared');

################################################################################
# Regression test: Check that urgent floating containers work properly (ticket
# #821)
################################################################################

    $tmp = fresh_workspace;
    my $floating_win = open_floating_window;

# switch away
    fresh_workspace;

    set_urgency($floating_win, 1, $type);
    sync_with_i3;

    cmd "workspace $tmp";

    does_i3_live;

###############################################################################
# Check if the urgency hint is still set when the urgent window is killed
###############################################################################

    my $ws1 = fresh_workspace;
    my $ws2 = fresh_workspace;
    cmd "workspace $ws1";
    $w1 = open_window;
    $w2 = open_window;
    cmd "workspace $ws2";
    sync_with_i3;
    set_urgency($w1, 1, $type);
    sync_with_i3;
    cmd '[id="' . $w1->id . '"] kill';
    sync_with_i3;
    my $w = get_ws($ws1);
    is($w->{urgent}, 0, 'Urgent flag no longer set after killing the window ' .
       'from another workspace');

##############################################################################
# Check if urgent flag can be unset if we move the window out of the container
##############################################################################
    $tmp = fresh_workspace;
    cmd 'layout tabbed';
    $w1 = open_window;
    $w2 = open_window;
    sync_with_i3;
    cmd '[id="' . $w2->id . '"] focus';
    sync_with_i3;
    cmd 'split v';
    cmd 'layout stacked';
    my $w3 = open_window;
    sync_with_i3;
    cmd '[id="' . $w2->id . '"] focus';
    sync_with_i3;
    set_urgency($w3, 1, $type);
    sync_with_i3;
    cmd 'focus parent';
    sync_with_i3;
    cmd 'move right';
    cmd '[id="' . $w3->id . '"] focus';
    sync_with_i3;
    $ws = get_ws($tmp);
    ok(!$ws->{urgent}, 'urgent flag not set on workspace');

##############################################################################
# Regression test for #1187: Urgency hint moves to new workspace when moving
# a container to another workspace.
##############################################################################

    my $tmp_source = fresh_workspace;
    my $tmp_target = fresh_workspace;
    cmd 'workspace ' . $tmp_source;
    sync_with_i3;
    $w1 = open_window;
    $w2 = open_window;
    sync_with_i3;
    cmd '[id="' . $w1->id . '"] focus';
    sync_with_i3;
    cmd 'mark urgent_con';
    cmd '[id="' . $w2->id . '"] focus';
    set_urgency($w1, 1, $type);
    sync_with_i3;
    cmd '[con_mark="urgent_con"] move container to workspace ' . $tmp_target;
    sync_with_i3;
    my $source_ws = get_ws($tmp_source);
    my $target_ws = get_ws($tmp_target);
    ok(!$source_ws->{urgent}, 'Source workspace is no longer marked urgent');
    is($target_ws->{urgent}, 1, 'Target workspace is now marked urgent');

##############################################################################

    exit_gracefully($pid);
}

done_testing;
