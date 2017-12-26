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
#
# Verifies that the _NET_ACTIVE_WINDOW message only changes focus when the
# window is on a visible workspace and that focus changes properly update this
# property on the root window.
# ticket #774, bug still present in commit 1e49f1b08a3035c1f238fcd6615e332216ab582e
# ticket #1136, bug still present in commit fd07f989fdf441ef335245dd3436a70ff60e8896
use i3test;

sub send_net_active_window {
    my ($id, $source) = @_;

    $source = (((defined $source) && ($source eq 'pager')) ? 2 : 0);

    my $msg = pack "CCSLLLLLLL",
        X11::XCB::CLIENT_MESSAGE, # response_type
        32, # format
        0, # sequence
        $id, # destination window
        $x->atom(name => '_NET_ACTIVE_WINDOW')->id,
        $source,
        0,
        0,
        0,
        0;

    $x->send_event(0, $x->get_root_window(), X11::XCB::EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);
    sync_with_i3;
}

sub get_net_active_window {
    my $cookie = $x->get_property(
        0,
        $x->get_root_window(),
        $x->atom(name => '_NET_ACTIVE_WINDOW')->id,
        $x->atom(name => 'WINDOW')->id,
        0,
        4096,
    );
    my $reply = $x->get_property_reply($cookie->{sequence});
    my $len = $reply->{length};

    return -1 if $len == 0;
    return unpack("L", $reply->{value});

}

my $ws1 = fresh_workspace;
my $win1 = open_window;
my $win2 = open_window;

################################################################################
# Ensure that the _NET_ACTIVE_WINDOW ClientMessage works when windows are visible
################################################################################

is($x->input_focus, $win2->id, 'window 2 has focus');

send_net_active_window($win1->id);

is($x->input_focus, $win1->id, 'window 1 has focus');

################################################################################
# Switch to a different workspace and ensure sending the _NET_ACTIVE_WINDOW
# ClientMessage switches to that workspaces only if source indicates it is a
# pager and otherwise sets the urgent hint.
################################################################################

my $ws2 = fresh_workspace;
my $win3 = open_window;

is($x->input_focus, $win3->id, 'window 3 has focus');

send_net_active_window($win1->id, 'pager');

is($x->input_focus, $win1->id, 'focus switched to window 1 when message source was a pager');

cmd '[id="' . $win3->id . '"] focus';

send_net_active_window($win1->id);

is($x->input_focus, $win3->id,
    'focus did not switch to window 1 on a hidden workspace when message source was an application');

ok(get_ws($ws1)->{urgent}, 'urgent hint set on ws 1');


################################################################################
# Make sure the ClientMessage only works with managed windows, and specifying a
# window that is not managed does not crash i3 (#774)
################################################################################

my $dock = open_window(window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'));

send_net_active_window($dock->id);

does_i3_live;
is($x->input_focus, $win3->id, 'dock did not get input focus');

send_net_active_window($x->get_root_window());

does_i3_live;
is($x->input_focus, $win3->id, 'root window did not get input focus');

################################################################################
# Move a window to the scratchpad, send a _NET_ACTIVE_WINDOW for it and verify
# that focus is still unchanged.
################################################################################

my $scratch = open_window;

is($x->input_focus, $scratch->id, 'to-scratchpad window has focus');

cmd 'move scratchpad';

is($x->input_focus, $win3->id, 'focus reverted to window 3');

send_net_active_window($scratch->id);

is($x->input_focus, $win3->id, 'window 3 still focused');

################################################################################
# A scratchpad window should be shown if _NET_ACTIVE_WINDOW from a pager
# is received.
################################################################################

$scratch = open_window;

is($x->input_focus, $scratch->id, 'to-scratchpad window has focus');

cmd 'move scratchpad';

is($x->input_focus, $win3->id, 'focus reverted to window 3');

send_net_active_window($scratch->id, 'pager');

is($x->input_focus, $scratch->id, 'scratchpad window is shown');

################################################################################
# Verify that the _NET_ACTIVE_WINDOW property is updated on the root window
# correctly.
################################################################################

fresh_workspace;

sync_with_i3;

is(get_net_active_window(), 0, 'workspace content focus is indicated by the root property as "None" window');

my $win4 = open_window;

cmd '[id="' . $win4->id . '"] focus';

sync_with_i3;

is(get_net_active_window(), $win4->id, 'window 4 is indicated as focused by the root property');

# make a branch
open_window;
open_window;
cmd 'split h';
open_window;
cmd 'focus parent';

sync_with_i3;

is(get_net_active_window(), 0, 'branch focus is indicated by the root property as "None" window');

done_testing;
