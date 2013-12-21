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
# Verifies that the _NET_ACTIVE_WINDOW message only changes focus when the
# window is on a visible workspace.
# ticket #774, bug still present in commit 1e49f1b08a3035c1f238fcd6615e332216ab582e
# ticket #1136, bug still present in commit fd07f989fdf441ef335245dd3436a70ff60e8896
use i3test;

sub send_net_active_window {
    my ($id) = @_;

    my $msg = pack "CCSLLLLLLL",
        X11::XCB::CLIENT_MESSAGE, # response_type
        32, # format
        0, # sequence
        $id, # destination window
        $x->atom(name => '_NET_ACTIVE_WINDOW')->id,
        0,
        0,
        0,
        0,
        0;

    $x->send_event(0, $x->get_root_window(), X11::XCB::EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);
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
# ClientMessage has no effect anymore.
################################################################################

my $ws2 = fresh_workspace;
my $win3 = open_window;

is($x->input_focus, $win3->id, 'window 3 has focus');

send_net_active_window($win1->id);

is($x->input_focus, $win3->id, 'window 3 still has focus');

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

done_testing;
