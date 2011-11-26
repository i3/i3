#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression test: Focusing a dock window should just do nothing, not crash i3.
# See ticket http://bugs.i3wm.org/575
# Wrong behaviour manifested itself up to (including) commit
# 340592a532b5259c3a3f575de5a9639fad4d1459
#
use i3test;

fresh_workspace;

my $window = open_window(
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
);

cmd '[title="' . $window->name . '"] focus';

does_i3_live;

done_testing;
