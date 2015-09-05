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
# Test behavior of "resize <width> <height>" command.
# Ticket: #1727
# Bug still in: 4.10.2-1-gc0dbc5d
use i3test;

################################################################################
# Check that setting floating windows size works
################################################################################

my $tmp = fresh_workspace;

open_floating_window;

my @content = @{get_ws($tmp)->{floating_nodes}};
is(@content, 1, 'one floating node on this ws');

my $oldrect = $content[0]->{rect};

cmd 'resize set 100 px 250 px';

@content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok($content[0]->{rect}->{x}, '==', $oldrect->{x}, 'x untouched');
cmp_ok($content[0]->{rect}->{y}, '==', $oldrect->{y}, 'y untouched');
cmp_ok($content[0]->{rect}->{width}, '!=', $oldrect->{width}, 'width changed');
cmp_ok($content[0]->{rect}->{height}, '!=', $oldrect->{width}, 'height changed');
cmp_ok($content[0]->{rect}->{width}, '==', 100, 'width changed to 100 px');
cmp_ok($content[0]->{rect}->{height}, '==', 250, 'height changed to 250 px');

done_testing;
