#!perl
# vim:ts=4:sw=4:expandtab
#
# Checks if the 'move workspace' command works correctly
#
use i3test tests => 11;
use Time::HiRes qw(sleep);

my $i3 = i3("/tmp/nestedcons");

# We move the pointer out of our way to avoid a bug where the focus will
# be set to the window under the cursor
my $x = X11::XCB::Connection->new;
$x->root->warp_pointer(0, 0);

my $tmp = get_unused_workspace();
my $tmp2 = get_unused_workspace();
cmd "workspace $tmp";

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $first = open_empty_con($i3);
my $second = open_empty_con($i3);
ok(@{get_ws_content($tmp)} == 2, 'two containers on first ws');

cmd "workspace $tmp2";
ok(@{get_ws_content($tmp2)} == 0, 'no containers on second ws yet');

cmd "workspace $tmp";

cmd "move workspace $tmp2";
ok(@{get_ws_content($tmp)} == 1, 'one container on first ws anymore');
ok(@{get_ws_content($tmp2)} == 1, 'one container on second ws');
my ($nodes, $focus) = get_ws_content($tmp2);

is($focus->[0], $second, 'same container on different ws');

($nodes, $focus) = get_ws_content($tmp);
is($nodes->[0]->{focused}, 1, 'first container focused on first ws');

###################################################################
# check if floating cons are moved to new workspaces properly
# (that is, if they are floating on the target ws, too)
###################################################################

$tmp = get_unused_workspace();
$tmp2 = get_unused_workspace();
cmd "workspace $tmp";

cmd "open";
cmd "mode toggle";

my $ws = get_ws($tmp);
is(@{$ws->{nodes}}, 0, 'no nodes on workspace');
is(@{$ws->{floating_nodes}}, 1, 'one floating node on workspace');

cmd "move workspace $tmp2";

$ws = get_ws($tmp2);
is(@{$ws->{nodes}}, 0, 'no nodes on workspace');
is(@{$ws->{floating_nodes}}, 1, 'one floating node on workspace');

diag( "Testing i3, Perl $], $^X" );
