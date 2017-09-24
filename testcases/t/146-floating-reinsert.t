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
use i3test;

my $tmp = fresh_workspace;

my $left = open_window;
my $mid = open_window;

cmd 'split v';
my $bottom = open_window;

my ($nodes, $focus) = get_ws_content($tmp);

#############################################################################
# 1: open a floating window, get it mapped
#############################################################################

# Create a floating window
my $window = open_floating_window;
ok($window->mapped, 'Window is mapped');

($nodes, $focus) = get_ws_content($tmp);
is(@{$nodes->[1]->{nodes}}, 2, 'two windows in split con');

#############################################################################
# 2: make it tiling, see where it ends up
#############################################################################

cmd 'floating toggle';

($nodes, $focus) = get_ws_content($tmp);

is(@{$nodes->[1]->{nodes}}, 3, 'three windows in split con after floating toggle');

done_testing;
