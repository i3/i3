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
# Regression test: ensure floating containers restore their pre-fullscreen
# geometry/position after leaving fullscreen.

use i3test;

my $ws = fresh_workspace;
my $win = open_floating_window(rect => [120, 160, 320, 180]);

sub floating_rect {
    my ($nodes) = get_ws($ws);
    my $floating_con = $nodes->{floating_nodes}->[0];
    return $floating_con->{rect};
}

my $original_rect = floating_rect();

cmd 'fullscreen enable';
cmd 'fullscreen disable';

is_deeply(floating_rect(), $original_rect, 'floating rect restored after fullscreen toggle');

done_testing;
