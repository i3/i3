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
# Test that an assignment that unmaps a window does not disturb input focus.
# This can cause i3 focus to be an unmapped window and different than X focus
# which can lead to complications
# Ticket: #1283
# Bug still in: 4.8-24-g60070de
use i3test i3_autostart => 0;

my $config = <<'EOT';
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

for_window [class="^special_kill$"] kill
for_window [class="^special_scratchpad$"] move scratchpad
EOT

my $pid = launch_with_config($config);

my $win = open_window;

my $scratch_window = open_window(
    wm_class => 'special_scratchpad',
    dont_map => 1
);
$scratch_window->map;
sync_with_i3;

is($x->input_focus, $win->{id},
    'an assignment that sends a window to the scratchpad should not disturb focus');

my $kill_window = open_window(
    wm_class => 'special_kill',
    dont_map => 1
);
$kill_window->map;
sync_with_i3;

is($x->input_focus, $win->{id},
    'an assignment that kills a window should not disturb focus');

exit_gracefully($pid);

done_testing;
