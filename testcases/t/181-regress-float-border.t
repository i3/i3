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
# Regression test: Changing border style should not have an impact on the size
# (geometry) of the child window. See ticket http://bugs.i3wm.org/561
# Wrong behaviour manifested itself up to (including) commit
# d805d1bbeaf89e11f67c981f94c9f55bbb4b89d9
#
use i3test;

my $tmp = fresh_workspace;

my $win = open_floating_window(rect => [10, 10, 200, 100]);

my $geometry = $win->rect;
is($geometry->{width}, 200, 'width correct');
is($geometry->{height}, 100, 'height correct');

cmd 'border 1pixel';

$geometry = $win->rect;
is($geometry->{width}, 200, 'width correct');
is($geometry->{height}, 100, 'height correct');

################################################################################
# When in fullscreen mode, the original position must not be overwritten.
################################################################################

sub get_floating_con_rect {
    my ($nodes, $focus) = get_ws($tmp);
    my $floating_con = $nodes->{floating_nodes}->[0];
    return $floating_con->{rect};
}
my $old_rect = get_floating_con_rect();

cmd 'fullscreen';

is_deeply(get_floating_con_rect(), $old_rect, 'Rect the same after going into fullscreen');

cmd 'border pixel 2';

is_deeply(get_floating_con_rect(), $old_rect, 'Rect the same after changing border style');

done_testing;
