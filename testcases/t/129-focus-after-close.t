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
# Check if the focus is correctly restored after closing windows.
#
use i3test;
use List::Util qw(first);

my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $first = open_empty_con($i3);
my $second = open_empty_con($i3);

cmd 'split v';

my ($nodes, $focus) = get_ws_content($tmp);

ok(!$nodes->[1]->{focused}, 'split container not focused');
cmd 'focus parent';
($nodes, $focus) = get_ws_content($tmp);
ok($nodes->[1]->{focused}, 'split container focused after focus parent');

my $third = open_empty_con($i3);

isnt(get_focused($tmp), $second, 'different container focused');

# We have the following layout now (con is focused):
# .----------------.
# | split  |       |
# | .----. |  con  |
# | | cn | |       |
# | `----' |       |
# `----------------'

##############################################################
# see if the focus goes down to $first (not to its split parent)
# when closing $second
##############################################################

cmd 'kill';
sync_with_i3;

($nodes, $focus) = get_ws_content($tmp);
is($nodes->[1]->{nodes}->[0]->{id}, $second, 'second container found');
ok($nodes->[1]->{nodes}->[0]->{focused}, 'second container focused');

##############################################################
# another case, using a slightly different layout (regression)
##############################################################

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

cmd 'split v';
$first = open_empty_con($i3);
my $bottom = open_empty_con($i3);

cmd 'focus up';
cmd 'split h';
my $middle = open_empty_con($i3);
my $right = open_empty_con($i3);
cmd 'focus down';

# We have the following layout now (second is focused):
# .----------------------------.
# | .------------------------. |
# | | first | middle | right | |
# | `------------------------' |
# |----------------------------|
# |                            |
# |          second            |
# |                            |
# `----------------------------'

# Check if the focus is restored to $right when we close $second
cmd 'kill';

is(get_focused($tmp), $right, 'top right container focused (in focus stack)');

($nodes, $focus) = get_ws_content($tmp);
my $tr = first { $_->{id} eq $right } @{$nodes->[0]->{nodes}};
is($tr->{focused}, 1, 'top right container really has focus');

##############################################################
# check if focus is correct after closing an unfocused window
##############################################################

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$first = open_empty_con($i3);
$middle = open_empty_con($i3);
# XXX: the $right empty con will be filled with the x11 window we are creating afterwards
$right = open_empty_con($i3);
my $win = open_window({ background_color => '#00ff00' });

cmd qq|[con_id="$middle"] focus|;
$win->destroy;
sync_with_i3;

is(get_focused($tmp), $middle, 'middle container focused');

##############################################################
# check if the workspace container can be closed
##############################################################

$tmp = fresh_workspace;

my $window = open_window();

# one window opened on the current workspace
($nodes, $focus) = get_ws_content($tmp);
is(scalar @$nodes, 1, 'workspace contains one node');

# focus the workspace
cmd "focus parent";
cmd "focus parent";

# try to kill the workspace
cmd "kill";
sync_with_i3;

# the workspace should now be empty
($nodes, $focus) = get_ws_content($tmp);
is(scalar @$nodes, 0, 'workspace is empty');

################################################################################
# check if killing a workspace also closes floating windows.
################################################################################

$tmp = fresh_workspace;

$window = open_window;
my $floating_window = open_floating_window;

# one window opened on the current workspace
($nodes, $focus) = get_ws_content($tmp);
is(scalar @$focus, 2, 'workspace contains two nodes');

# focus the workspace
cmd "focus parent";
cmd "focus parent";

# try to kill the workspace
cmd "kill";
sync_with_i3;

# the workspace should now be empty
($nodes, $focus) = get_ws_content($tmp);
is(scalar @$focus, 0, 'workspace is empty');

##############################################################
# and now for something completely different:
# check if the pointer position is relevant when restoring focus
# (it should not be relevant, of course)
##############################################################

# TODO: add test code as soon as I can reproduce it

done_testing;
