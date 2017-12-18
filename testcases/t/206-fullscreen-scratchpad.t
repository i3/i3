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
#   Assure that no window is in fullscreen mode after showing a scratchpad window
# Bug still in: 4.5.1-54-g0f6b5fe

use i3test;

my $tmp = fresh_workspace;

##########################################################################################
# map two windows in one container, fullscreen one of them and then move it to scratchpad
##########################################################################################

my $first_win = open_window;
my $second_win = open_window;

# fullscreen the focused window
cmd 'fullscreen';

# see if the window really is in fullscreen mode
is_num_fullscreen($tmp, 1, 'amount of fullscreen windows after enabling fullscreen');

# move window to scratchpad
cmd 'move scratchpad';

###############################################################################
# show the scratchpad window again; it should not be in fullscreen mode anymore
###############################################################################

# show window from scratchpad
cmd 'scratchpad show';

# switch window back to tiling mode
cmd 'floating toggle';

# see if no window is in fullscreen mode
is_num_fullscreen($tmp, 0, 'amount of fullscreen windows after showing previously fullscreened scratchpad window');

########################################################################################
# move a window to scratchpad, focus parent container, make it fullscreen, focus a child
########################################################################################

# make layout tabbed
cmd 'layout tabbed';

# move one window to scratchpad
cmd 'move scratchpad';

# focus parent
cmd 'focus parent';

# fullscreen the container
cmd 'fullscreen';

# focus child
cmd 'focus child';

# see if the window really is in fullscreen mode
is_num_fullscreen($tmp, 1, 'amount of fullscreen windows after enabling fullscreen on parent');

##########################################################################
# show a scratchpad window; no window should be in fullscreen mode anymore
##########################################################################

# show the scratchpad window
cmd 'scratchpad show';

# see if no window is in fullscreen mode
is_num_fullscreen($tmp, 0, 'amount of fullscreen windows after showing a scratchpad window while a parent container was in fullscreen mode');

done_testing;
