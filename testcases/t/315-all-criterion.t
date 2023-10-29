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
# Tests all kinds of matching methods
#
use i3test;

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

# Open a new window
my $window = open_window;
my $content = get_ws_content($tmp);
ok(@{$content} == 1, 'window mapped');
my $win = $content->[0];

######################################################################
# check that simple matching works.
######################################################################
cmd '[all] kill';

sync_with_i3;

is_num_children($tmp, 0, 'window killed');

######################################################################
# check that simple matching against multiple windows works.
######################################################################

$tmp = fresh_workspace;

my $left = open_window;
ok($left->mapped, 'left window mapped');

my $right = open_window;
ok($right->mapped, 'right window mapped');

# two windows should be here
is_num_children($tmp, 2, 'two windows opened');

cmd '[all] kill';

sync_with_i3;

is_num_children($tmp, 0, 'two windows killed');

######################################################################
# check that multiple criteria work are checked with a logical AND,
# not a logical OR (that is, matching is not cancelled after the first
# criterion matches).
######################################################################

$tmp = fresh_workspace;

my $left = open_window(name => 'left');
ok($left->mapped, 'left window mapped');

my $right = open_window(name => 'right');
ok($right->mapped, 'right window mapped');

# two windows should be here
is_num_children($tmp, 2, 'two windows opened');

cmd '[all title="left"] kill';

sync_with_i3;

is_num_children($tmp, 1, 'one window still there');

cmd '[all] kill';

sync_with_i3;

is_num_children($tmp, 0, 'all windows killed');

done_testing;
