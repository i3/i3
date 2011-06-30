#!perl
# vim:ts=4:sw=4:expandtab

use i3test;
use X11::XCB qw(:all);
use X11::XCB::Connection;

my $x = X11::XCB::Connection->new;

my $tmp = fresh_workspace;

#############################################################################
# 1: see if focus stays the same when toggling tiling/floating mode
#############################################################################

my $first = open_standard_window($x);
my $second = open_standard_window($x);

is($x->input_focus, $second->id, 'second window focused');

cmd 'floating enable';
cmd 'floating disable';

is($x->input_focus, $second->id, 'second window still focused after mode toggle');

#############################################################################
# 2: see if focus stays on the current floating window if killing another
# floating window
#############################################################################

$tmp = fresh_workspace;

$first = open_standard_window($x);    # window 2
$second = open_standard_window($x);   # window 3
my $third = open_standard_window($x); # window 4

is($x->input_focus, $third->id, 'last container focused');

cmd 'floating enable';

cmd '[id="' . $second->id . '"] focus';

is($x->input_focus, $second->id, 'second con focused');

cmd 'floating enable';

# now kill the third one (it's floating). focus should stay unchanged
cmd '[id="' . $third->id . '"] kill';

sleep 0.25;

is($x->input_focus, $second->id, 'second con still focused after killing third');


#############################################################################
# 3: see if the focus gets reverted correctly when closing floating clients
# (first to the next floating client, then to the last focused tiling client)
#############################################################################

$tmp = fresh_workspace;

$first = open_standard_window($x);    # window 5
$second = open_standard_window($x);   # window 6
my $third = open_standard_window($x); # window 7

is($x->input_focus, $third->id, 'last container focused');

cmd 'floating enable';

cmd '[id="' . $second->id . '"] focus';

is($x->input_focus, $second->id, 'second con focused');

cmd 'floating enable';

# now kill the second one. focus should fall back to the third one, which is
# also floating
cmd 'kill';

sleep 0.25;

is($x->input_focus, $third->id, 'third con focused');

cmd 'kill';

sleep 0.25;

is($x->input_focus, $first->id, 'first con focused after killing all floating cons');

done_testing;
