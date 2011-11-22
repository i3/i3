#!perl
# vim:ts=4:sw=4:expandtab

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

sync_with_i3;

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

sync_with_i3;

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

sync_with_i3;

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

sync_with_i3;

is($x->input_focus, $second->id, 'second container focused');

cmd 'floating enable';

is($x->input_focus, $second->id, 'second container focused');

cmd 'focus tiling';

sync_with_i3;

is($x->input_focus, $first->id, 'first (tiling) container focused');

cmd 'focus floating';

sync_with_i3;

is($x->input_focus, $second->id, 'second (floating) container focused');

cmd 'focus floating';

sync_with_i3;

is($x->input_focus, $second->id, 'second (floating) container still focused');

cmd 'focus mode_toggle';

sync_with_i3;

is($x->input_focus, $first->id, 'first (tiling) container focused');

cmd 'focus mode_toggle';

sync_with_i3;

is($x->input_focus, $second->id, 'second (floating) container focused');

#############################################################################
# 6: see if switching floating focus using the focus left/right command works
#############################################################################

$tmp = fresh_workspace;

$first = open_floating_window({ background_color => '#ff0000' });# window 10
$second = open_floating_window({ background_color => '#00ff00' }); # window 11
$third = open_floating_window({ background_color => '#0000ff' }); # window 12

sync_with_i3;

is($x->input_focus, $third->id, 'third container focused');

cmd 'focus left';

sync_with_i3;

is($x->input_focus, $second->id, 'second container focused');

cmd 'focus left';

sync_with_i3;

is($x->input_focus, $first->id, 'first container focused');

cmd 'focus left';

sync_with_i3;

is($x->input_focus, $third->id, 'focus wrapped to third container');

cmd 'focus right';

sync_with_i3;

is($x->input_focus, $first->id, 'focus wrapped to first container');

cmd 'focus right';

sync_with_i3;

is($x->input_focus, $second->id, 'focus on second container');

done_testing;
