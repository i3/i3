#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Test for the startup notification protocol.
#

use i3test;

######################################################################
# 1) initiate startup, create window which uses setinputfocus
######################################################################

my $first_ws = fresh_workspace;
my $win = open_window();

# This is what Emacs is doing in some cases, e.g. if you run:
#   emacs -Q --eval '(progn (x-focus-frame nil) (kill-emacs))'
# After this command, your i3 is completely screwed and you need your
# mouse to get back to work, nothing to do with your keyboard
# anymore...
X11::XCB::set_input_focus($x, X11::XCB::INPUT_FOCUS_PARENT, $win->id, X11::XCB::TIME_CURRENT_TIME);
# Otherwise the test is a little bit flaky.  I tried to wait for
# FOCUS_IN event, but somehow it's not enough...
sleep(0.2);
is($x->input_focus, $win->id, 'focus is on the new window');

######################################################################
# 2) close the app that owns the window
######################################################################

cmd 'kill';
wait_for_unmap($win);
sync_with_i3;
isnt($x->input_focus, 0, 'focus falls back to the EWMH window');

done_testing;
