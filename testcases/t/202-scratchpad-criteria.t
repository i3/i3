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
# Verifies that using criteria to address scratchpad windows works.
use i3test;

my $i3 = i3(get_socket_path());

#####################################################################
# Verify that using scratchpad show with criteria works as expected:
# - When matching a scratchpad window which is visible,
#   it should hide it.
# - When matching a scratchpad window which is on __i3_scratch,
#   it should show it.
# - When matching a non-scratchpad window, it should be a no-op.
# - When matching a scratchpad window,
#   non-matching windows shouldn't appear
######################################################################

my $tmp = fresh_workspace;

my $third_window = open_window(name => 'scratch-match');
cmd 'move scratchpad';

#####################################################################
# Verify that using 'scratchpad show' without any matching windows is
# a no-op.
#####################################################################
my $old_focus = get_focused($tmp);

cmd '[title="nomatch"] scratchpad show';

is(get_focused($tmp), $old_focus, 'non-matching criteria have no effect');

#####################################################################
# Verify that we can use criteria to show a scratchpad window.
#####################################################################
cmd '[title="scratch-match"] scratchpad show';

my $scratch_focus = get_focused($tmp);
isnt($scratch_focus, $old_focus, 'matching criteria works');

# Check that the window was centered and resized too.
my $tree = $i3->get_tree->recv;
my $ws = get_ws($tmp);
my $scratchrect = $ws->{floating_nodes}->[0]->{rect};
my $output = $tree->{nodes}->[1];
my $outputrect = $output->{rect};

is($scratchrect->{width}, $outputrect->{width} * 0.5, 'scratch width is 50%');
is($scratchrect->{height}, $outputrect->{height} * 0.75, 'scratch height is 75%');
is($scratchrect->{x},
   ($outputrect->{width} / 2) - ($scratchrect->{width} / 2),
   'scratch window centered horizontally');
is($scratchrect->{y},
   ($outputrect->{height} / 2 ) - ($scratchrect->{height} / 2),
   'scratch window centered vertically');

cmd '[title="scratch-match"] scratchpad show';

isnt(get_focused($tmp), $scratch_focus, 'matching criteria works');
is(get_focused($tmp), $old_focus, 'focus restored');


#####################################################################
# Verify that we cannot use criteria to show a non-scratchpad window.
#####################################################################
my $tmp2 = fresh_workspace;
my $non_scratch_window = open_window(name => 'non-scratch');
cmd "workspace $tmp";
is(get_focused($tmp), $old_focus, 'focus still ok');
cmd '[title="non-scratch"] scratchpad show';
is(get_focused($tmp), $old_focus, 'focus unchanged');

#####################################################################
# Verify that non-matching windows doesn't appear
#####################################################################
# Subroutine to clear scratchpad
sub clear_scratchpad {
    while (scalar @{get_ws('__i3_scratch')->{floating_nodes}}) {
        cmd 'scratchpad show';
        cmd 'kill';
    }
}

#Start from an empty fresh workspace
my $empty_ws = fresh_workspace;
cmd "workspace $empty_ws";

my $no_focused = get_focused($empty_ws);
cmd '[title="nothingmatchthistitle"] scratchpad show';
#Check nothing match
is(get_focused($empty_ws), $no_focused, "no window to focus on");

clear_scratchpad;

open_window(name => "my-scratch-window");
my $w1_focus = get_focused($empty_ws);
cmd 'move scratchpad';
cmd '[title="my-scratch-window"] scratchpad show';
#Check we created and shown a scratchpad window
is(get_focused($empty_ws), $w1_focus, "focus on scratchpad window");

#Switching workspace
my $empty_ws2 = fresh_workspace;
cmd "workspace $empty_ws2";
open_window(name => "my-second-scratch-window");

my $w2_focus = get_focused($empty_ws2);
cmd 'move scratchpad';
cmd '[title="my-second-scratch-window"] scratchpad show';

#Check we got the correct window
is(get_focused($empty_ws2), $w2_focus, "focus is on second window");

#####################################################################
# Verify that 'scratchpad show' correctly hide multiple scratchpad
# windows
#####################################################################
clear_scratchpad;

sub check_floating {
    my($rws, $n) = @_;
    my $ws = get_ws($rws);
    is(scalar @{$ws->{nodes}}, 0, 'no windows on ws');
    is(scalar @{$ws->{floating_nodes}}, $n, "$n floating windows on ws");
}

my $empty_ws3 = fresh_workspace;
cmd "workspace $empty_ws3";

check_floating($empty_ws3, 0);

#Creating two scratchpad windows
open_window(name => "toggle-1");
cmd 'move scratchpad';
open_window(name => "toggle-2");
cmd 'move scratchpad';
check_floating($empty_ws3, 0);
#Showing both
cmd '[title="toggle-"] scratchpad show';

check_floating($empty_ws3, 2);

#Hiding both
cmd '[title="toggle-"] scratchpad show';
check_floating($empty_ws3, 0);

#Showing both again
cmd '[title="toggle-"] scratchpad show';
check_floating($empty_ws3, 2);


#Hiding one
cmd 'scratchpad show';
check_floating($empty_ws3, 1);

#Hiding the last
cmd 'scratchpad show';
check_floating($empty_ws3, 0);

done_testing;
