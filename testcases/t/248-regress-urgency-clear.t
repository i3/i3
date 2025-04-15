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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Ensures the urgency hint is cleared properly in the case where i3 set it (due
# to focus_on_window_activation=urgent), hence the application not clearing it.
# Ticket: #1825
# Bug still in: 4.10.3-253-g03799dd
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

focus_on_window_activation urgent
EOT

sub send_net_active_window {
    my ($id) = @_;

    my $msg = pack "CCSLLLLLLL",
        X11::XCB::CLIENT_MESSAGE, # response_type
        32, # format
        0, # sequence
        $id, # destination window
        $x->atom(name => '_NET_ACTIVE_WINDOW')->id,
        0, # source
        0, 0, 0, 0;

    $x->send_event(0, $x->get_root_window(), X11::XCB::EVENT_MASK_SUBSTRUCTURE_REDIRECT, $msg);
}

my $ws = fresh_workspace;
my $first = open_window;
my $second = open_window;

send_net_active_window($first->id);
sync_with_i3;
is($x->input_focus, $second->id, 'second window still focused');

cmd '[urgent=latest] focus';
sync_with_i3;
is($x->input_focus, $first->id, 'first window focused');

cmd 'focus right';
sync_with_i3;
is($x->input_focus, $second->id, 'second window focused again');

cmd '[urgent=latest] focus';
sync_with_i3;
is($x->input_focus, $second->id, 'second window still focused');

done_testing;
