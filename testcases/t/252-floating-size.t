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
# Test behavior of "resize <width> <height>" command.
# Ticket: #1727
# Bug still in: 4.10.2-1-gc0dbc5d
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1333x999+0+0
workspace ws output fake-0
EOT

################################################################################
# Init variables used for all tests.
################################################################################

my $tmp = fresh_workspace;
open_floating_window;
my @content = @{get_ws($tmp)->{floating_nodes}};
is(@content, 1, 'one floating node on this ws');
my $oldrect = $content[0]->{rect};

sub do_test {
    my ($width, $height) = @_;

    cmp_ok($content[0]->{rect}->{x}, '==', $oldrect->{x}, 'x unchanged');
    cmp_ok($content[0]->{rect}->{y}, '==', $oldrect->{y}, 'y unchanged');

    @content = @{get_ws($tmp)->{floating_nodes}};
    if ($width) {
        cmp_ok($content[0]->{rect}->{width}, '==', $width, "width changed to $width px");
    } else {
        cmp_ok($content[0]->{rect}->{width}, '==', $oldrect->{width}, 'width unchanged');
    }
    if ($height) {
        cmp_ok($content[0]->{rect}->{height}, '==', $height, "height changed to $height px");
    } else {
        cmp_ok($content[0]->{rect}->{height}, '==', $oldrect->{height}, 'height unchanged');
    }
    $oldrect = $content[0]->{rect};
}

################################################################################
# Check that setting floating windows size works
################################################################################

cmd 'resize set 100 px 250 px';
do_test(100, 250);

################################################################################
# Same but with ppt instead of px
################################################################################

cmd 'resize set 33 ppt 20 ppt';
do_test(int(0.33 * 1333), int(0.2 * 999));

################################################################################
# Mix ppt and px in a single resize set command
################################################################################

cmd 'resize set 44 ppt 111 px';
do_test(int(0.44 * 1333), 111);

cmd 'resize set 222 px 100 ppt';
do_test(222, 999);

################################################################################
# Zero is interpreted as no change.
# See issue: #3276.
################################################################################

cmd 'resize set 0 px 333 px';
do_test(0, 333);

cmd 'resize set 333 px 0 ppt';
do_test(333, 0);

cmd 'resize set 0 px 0 ppt';
do_test(0, 0);

cmd 'resize set 100 ppt 0 px';
do_test(1333, 0);

################################################################################
# Use 'width' and 'height' keywords.
# See issue: #3275.
################################################################################

cmd 'resize set width 200 px';
do_test(200, 0);

cmd 'resize set height 200 px';
do_test(0, 200);

cmd 'resize set width 300 px height 300 px';
do_test(300, 300);

# ppt + keyword used only for height
cmd 'resize set 100 ppt height 100 px';
do_test(1333, 100);

done_testing;
