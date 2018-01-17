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
# Test behavior of "resize <width> <height>" command.
# Ticket: #1727
# Bug still in: 4.10.2-1-gc0dbc5d
use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1333x999+0+0
workspace ws output fake-0
EOT

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

################################################################################
# Same but with ppt instead of px
################################################################################

kill_all_windows;
$tmp = 'ws';
cmd "workspace $tmp";
open_floating_window;

@content = @{get_ws($tmp)->{floating_nodes}};
is(@content, 1, 'one floating node on this ws');

$oldrect = $content[0]->{rect};

cmd 'resize set 33 ppt 20 ppt';
my $expected_width = int(0.33 * 1333);
my $expected_height = int(0.2 * 999);

@content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok($content[0]->{rect}->{x}, '==', $oldrect->{x}, 'x untouched');
cmp_ok($content[0]->{rect}->{y}, '==', $oldrect->{y}, 'y untouched');
cmp_ok($content[0]->{rect}->{width}, '!=', $oldrect->{width}, 'width changed');
cmp_ok($content[0]->{rect}->{height}, '!=', $oldrect->{width}, 'height changed');
cmp_ok($content[0]->{rect}->{width}, '==', $expected_width, "width changed to $expected_width px");
cmp_ok($content[0]->{rect}->{height}, '==', $expected_height, "height changed to $expected_height px");

################################################################################
# Mix ppt and px in a single resize set command
################################################################################

cmd 'resize set 44 ppt 111 px';
$expected_width = int(0.44 * 1333);
$expected_height = 111;

@content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok($content[0]->{rect}->{x}, '==', $oldrect->{x}, 'x untouched');
cmp_ok($content[0]->{rect}->{y}, '==', $oldrect->{y}, 'y untouched');
cmp_ok($content[0]->{rect}->{width}, '==', $expected_width, "width changed to $expected_width px");
cmp_ok($content[0]->{rect}->{height}, '==', $expected_height, "height changed to $expected_height px");

cmd 'resize set 222 px 100 ppt';
$expected_width = 222;
$expected_height = 999;

@content = @{get_ws($tmp)->{floating_nodes}};
cmp_ok($content[0]->{rect}->{x}, '==', $oldrect->{x}, 'x untouched');
cmp_ok($content[0]->{rect}->{y}, '==', $oldrect->{y}, 'y untouched');
cmp_ok($content[0]->{rect}->{width}, '==', $expected_width, "width changed to $expected_width px");
cmp_ok($content[0]->{rect}->{height}, '==', $expected_height, "height changed to $expected_height px");

done_testing;
