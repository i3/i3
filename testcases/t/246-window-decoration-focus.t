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
# Ensure that hovering over the window decoration of a window causes it to focus
# correctly.
# Ticket: #1056
# Bug still in: 4.10.2-174-g8029ff0
use i3test;

my ($ws, $A, $B, $C, $target, $y);
my @cons;

# ==================================================================================
# Given the following layout (= denotes the window decoration),
# when moving the mouse from 1 to 2,
# then the C should be focused.
#
# This should especially be the case if B is the focus head of its vertically split parent container.
#
# +===A===+===B===+
# |       |       |
# |     1 +=2=C===+
# |       |       |
# +-------+-------+
#
# ==================================================================================

$ws = fresh_workspace;

open_window;
$B = open_window;
cmd 'split v';
open_window;
$target = get_focused($ws);

@cons = @{get_ws($ws)->{nodes}};
$A = $cons[0];
$C = $cons[1]->{nodes}[1];

$y = $C->{rect}->{y} - 0.5 * $C->{deco_rect}->{height};

# make sure that B is the focus head of its parent
cmd '[id="' . $B->{id} . '"] focus';

# move pointer to position 1
$x->root->warp_pointer($C->{rect}->{x} - 0.5 * $A->{rect}->{width}, $y);
sync_with_i3;

# move pointer to position 2
$x->root->warp_pointer($C->{rect}->{x} + 0.5 * $C->{rect}->{width}, $y);
sync_with_i3;

is(get_focused($ws), $target, 'focus switched to container C');

# ==================================================================================
# Given a tabbed container when the mouse is moved onto the window decoration
# then the focus head of the tabbed container is focused regardless of which particular
# tab's decoration the mouse is on.
#
# +=========+=========+
# |         |         |
# |       1 +=2==|****| <- tab to the right is focus head of tabbed container
# |         |         |
# +---------+---------+
#
# ==================================================================================

$ws = fresh_workspace;

open_window;
open_window;
cmd 'split v';
open_window;
cmd 'split h';
open_window;
$target = get_focused($ws);
cmd 'layout tabbed';

@cons = @{get_ws($ws)->{nodes}};
$A = $cons[0];
$B = $cons[1]->{nodes}[1]->{nodes}[1];

$y = $B->{rect}->{y} - 0.5 * $B->{deco_rect}->{height};

$x->root->warp_pointer($B->{rect}->{x} - 0.5 * $A->{rect}->{width}, $y);
sync_with_i3;

$x->root->warp_pointer($B->{rect}->{x} + 0.2 * $B->{rect}->{width}, $y);
sync_with_i3;

is(get_focused($ws), $target, 'focus switched to the focus head of the tabbed container');

# ==================================================================================
# Given a stacked container when the mouse is moved onto the window decoration
# then the focus head of the stacked container is focused regardless of which particular
# tab's decoration the mouse is on.
#
# +=========+=========+
# |         |         |
# |       1 +=2=======+
# |         +*********+ <- the lower tab is the focus head of the stacked container
# |         |         |
# +---------+---------+
#
# ==================================================================================

$ws = fresh_workspace;

open_window;
open_window;
cmd 'split v';
open_window;
cmd 'split h';
open_window;
$target = get_focused($ws);
cmd 'layout stacked';

@cons = @{get_ws($ws)->{nodes}};
$A = $cons[0];
$B = $cons[1]->{nodes}[1]->{nodes}[0];
$C = $cons[1]->{nodes}[1]->{nodes}[1];

$y = $B->{rect}->{y} - 1.5 * $B->{deco_rect}->{height};

$x->root->warp_pointer($B->{rect}->{x} - 0.5 * $A->{rect}->{width}, $y);
sync_with_i3;

$x->root->warp_pointer($B->{rect}->{x} + 0.5 * $B->{rect}->{width}, $y);
sync_with_i3;

is(get_focused($ws), $target, 'focus switched to the focus head of the stacked container');

# ==================================================================================

done_testing;
