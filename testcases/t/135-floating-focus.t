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

use i3test;

my $tmp = fresh_workspace;

#############################################################################
# 1: see if focus stays the same when toggling tiling/floating mode
#############################################################################

my $first = open_window;
my $second = open_window;

is($x->input_focus, $second->id, 'second window focused');

cmd 'floating enable';
cmd 'floating disable';

is($x->input_focus, $second->id, 'second window still focused after mode toggle');

#############################################################################
# 2: see if focus stays on the current floating window if killing another
# floating window
#############################################################################

$tmp = fresh_workspace;

$first = open_window;    # window 2
$second = open_window;   # window 3
my $third = open_window; # window 4

is($x->input_focus, $third->id, 'last container focused');

cmd 'floating enable';

cmd '[id="' . $second->id . '"] focus';

is($x->input_focus, $second->id, 'second con focused');

cmd 'floating enable';

# now kill the third one (it's floating). focus should stay unchanged
cmd '[id="' . $third->id . '"] kill';

wait_for_unmap($third);

is($x->input_focus, $second->id, 'second con still focused after killing third');


#############################################################################
# 3: see if the focus gets reverted correctly when closing floating clients
# (first to the next floating client, then to the last focused tiling client)
#############################################################################

$tmp = fresh_workspace;

$first = open_window({ background_color => '#ff0000' });    # window 5
$second = open_window({ background_color => '#00ff00' });   # window 6
$third = open_window({ background_color => '#0000ff' }); # window 7

is($x->input_focus, $third->id, 'last container focused');

cmd 'floating enable';

cmd '[id="' . $second->id . '"] focus';

is($x->input_focus, $second->id, 'second con focused');

cmd 'floating enable';

# now kill the second one. focus should fall back to the third one, which is
# also floating
cmd 'kill';
wait_for_unmap($second);

is($x->input_focus, $third->id, 'third con focused');

cmd 'kill';
wait_for_unmap($third);

is($x->input_focus, $first->id, 'first con focused after killing all floating cons');

#############################################################################
# 4: same test as 3, but with another split con
#############################################################################

$tmp = fresh_workspace;

$first = open_window({ background_color => '#ff0000' });    # window 5
cmd 'split v';
cmd 'layout stacked';
$second = open_window({ background_color => '#00ff00' });   # window 6
$third = open_window({ background_color => '#0000ff' }); # window 7

is($x->input_focus, $third->id, 'last container focused');

cmd 'floating enable';

cmd '[id="' . $second->id . '"] focus';

is($x->input_focus, $second->id, 'second con focused');

cmd 'floating enable';

sync_with_i3;

# now kill the second one. focus should fall back to the third one, which is
# also floating
cmd 'kill';
wait_for_unmap($second);

is($x->input_focus, $third->id, 'third con focused');

cmd 'kill';
wait_for_unmap($third);

is($x->input_focus, $first->id, 'first con focused after killing all floating cons');

#############################################################################
# 5: see if the 'focus tiling' and 'focus floating' commands work
#############################################################################

$tmp = fresh_workspace;

$first = open_window({ background_color => '#ff0000' });    # window 8
$second = open_window({ background_color => '#00ff00' });   # window 9

is($x->input_focus, $second->id, 'second container focused');

cmd 'floating enable';

is($x->input_focus, $second->id, 'second container focused');

cmd 'focus tiling';

is($x->input_focus, $first->id, 'first (tiling) container focused');

cmd 'focus floating';

is($x->input_focus, $second->id, 'second (floating) container focused');

cmd 'focus floating';

is($x->input_focus, $second->id, 'second (floating) container still focused');

cmd 'focus mode_toggle';

is($x->input_focus, $first->id, 'first (tiling) container focused');

cmd 'focus mode_toggle';

is($x->input_focus, $second->id, 'second (floating) container focused');

#############################################################################
# 6: see if switching floating focus using the focus left/right command works
#############################################################################

$tmp = fresh_workspace;

$first = open_floating_window({ background_color => '#ff0000' });# window 10
$second = open_floating_window({ background_color => '#00ff00' }); # window 11
$third = open_floating_window({ background_color => '#0000ff' }); # window 12

is($x->input_focus, $third->id, 'third container focused');

cmd 'focus left';

is($x->input_focus, $second->id, 'second container focused');

cmd 'focus left';

is($x->input_focus, $first->id, 'first container focused');

cmd 'focus left';

is($x->input_focus, $third->id, 'focus wrapped to third container');

cmd 'focus right';

is($x->input_focus, $first->id, 'focus wrapped to first container');

cmd 'focus right';

is($x->input_focus, $second->id, 'focus on second container');

#############################################################################
# 7: verify that focusing the parent of a window inside a floating con goes
# up to the grandparent (workspace) and that focusing child from the ws
# goes back down to the child of the floating con
#############################################################################

$tmp = fresh_workspace;

my $tiled = open_window;
my $floating = open_floating_window;
is($x->input_focus, $floating->id, 'floating window focused');

cmd 'focus parent';

is(get_ws($tmp)->{focused}, 1, 'workspace is focused');
cmd 'focus child';

is($x->input_focus, $floating->id, 'floating window focused');

#############################################################################
# 8: verify that focusing a floating window raises it to the top.
# This test can't verify that the floating container is visually on top, just
# that it is placed on the tail of the floating_head.
# See issue: 2572
#############################################################################

$tmp = fresh_workspace;

$first = open_floating_window;
$second = open_floating_window;

is($x->input_focus, $second->id, 'second floating window focused');
my $ws = get_ws($tmp);
is($ws->{floating_nodes}->[1]->{nodes}->[0]->{window}, $second->id, 'second on top');
is($ws->{floating_nodes}->[0]->{nodes}->[0]->{window}, $first->id, 'first behind');

cmd '[id=' . $first->id . '] focus';

is($x->input_focus, $first->id, 'first floating window focused');
$ws = get_ws($tmp);
is($ws->{floating_nodes}->[1]->{nodes}->[0]->{window}, $first->id, 'first on top');
is($ws->{floating_nodes}->[0]->{nodes}->[0]->{window}, $second->id, 'second behind');

done_testing;
