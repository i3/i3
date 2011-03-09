#!perl
# vim:ts=4:sw=4:expandtab
#
# Regression test: level up should be a noop during fullscreen mode
#
use X11::XCB qw(:all);
use i3test;

BEGIN {
    use_ok('X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;
my $i3 = i3("/tmp/nestedcons");

my $tmp = fresh_workspace;

#####################################################################
# open a window, verify it’s not in fullscreen mode
#####################################################################

my $win = open_standard_window($x);

my $nodes = get_ws_content $tmp;
is(@$nodes, 1, 'exactly one client');
is($nodes->[0]->{fullscreen_mode}, 0, 'client not fullscreen');

#####################################################################
# make it fullscreen
#####################################################################

cmd 'nop making fullscreen';
cmd 'fullscreen';

my $nodes = get_ws_content $tmp;
is($nodes->[0]->{fullscreen_mode}, 1, 'client fullscreen now');

#####################################################################
# send level up, try to un-fullscreen
#####################################################################
cmd 'level up';
cmd 'fullscreen';

my $nodes = get_ws_content $tmp;
is($nodes->[0]->{fullscreen_mode}, 0, 'client not fullscreen any longer');

does_i3_live;

done_testing;
