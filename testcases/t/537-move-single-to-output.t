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
# Tests that windows inside containers with a single child do not jump
# over other containers and erratically move across outputs.
# Ticket: #2466
# Bug still in: 4.14-63-g75d11820
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 640x480+0+0,640x480+640+0

workspace left output fake-0
workspace right output fake-1
EOT

cmd 'workspace right';

# ┌───────────────────────────┐ ┌───────────────────────────┐
# │         Output 1          │ │     Output 2 - splith     │
# │                           │ │┌───────────┐┌────────────┐│
# │                           │ ││           ││   splitv   ││
# │                           │ ││           ││╔══════════╗││
# │                           │ ││           ││║          ║││
# │     (empty workspace)     │ ││   first   ││║  second  ║││
# │                           │ ││           ││║          ║││
# │                           │ ││           ││║          ║││
# │                           │ ││           ││╚══════════╝││
# │                           │ │└───────────┘└────────────┘│
# └───────────────────────────┘ └───────────────────────────┘
#
# Moving "second" left shouldn't cause it to jump over to output 1.

my $first = open_window;
my $second = open_window;

# Put the second window into its own splitv container
cmd 'split v';

is_num_children('left', 0, 'No children on left');
is_num_children('right', 2, 'Two children on right');

# Move the second window to the left
cmd 'move left';

is_num_children('left', 0, 'Still no children on left');
is_num_children('right', 2, 'Still two children on right');

done_testing;
