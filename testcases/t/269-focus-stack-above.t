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
# Verifies a ConfigureWindow request with stack-mode=Above is translated into
# focusing the target window by i3.
# Ticket: #2708
# Bug still in: 4.13-207-gafdf6792
use i3test;
use X11::XCB qw(CONFIG_WINDOW_STACK_MODE STACK_MODE_ABOVE);

my $ws = fresh_workspace;
my $left_window = open_window;
my $right_window = open_window;

is($x->input_focus, $right_window->id, 'right window has focus');
my $old_focus = get_focused($ws);

$x->configure_window($left_window->id, CONFIG_WINDOW_STACK_MODE, (STACK_MODE_ABOVE));
$x->flush;

sync_with_i3;

is($x->input_focus, $left_window->id, 'left window has focus');
isnt(get_focused($ws), $old_focus, 'right window is no longer focused');

################################################################################
# Verify the ConfigureWindow request is only applied when on the active
# workspace.
################################################################################

$ws = fresh_workspace;
my $new_window = open_window;

is($x->input_focus, $new_window->id, 'new window has focus');
$x->configure_window($left_window->id, CONFIG_WINDOW_STACK_MODE, (STACK_MODE_ABOVE));
$x->flush;

sync_with_i3;

is($x->input_focus, $new_window->id, 'new window still has focus');

done_testing;
