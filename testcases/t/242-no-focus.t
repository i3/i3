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
# Test the 'no_focus' directive.
# Ticket: #1416
use i3test i3_autostart => 0;

#####################################################################
# 1: open a window and check that it takes focus
#####################################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $pid = launch_with_config($config);
 
my $ws = fresh_workspace;
my $first = open_window;
my $focused = get_focused($ws);
my $second = open_window;

isnt(get_focused($ws), $focused, 'focus has changed');

exit_gracefully($pid);

#####################################################################
# 2: open a window matched by a no_focus directive and check that
#    it doesn't take focus
#####################################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

no_focus [instance=notme]
EOT

my $pid = launch_with_config($config);
 
my $ws = fresh_workspace;
my $first = open_window;
my $focused = get_focused($ws);
my $second = open_window(wm_class => 'notme');

is(get_focused($ws), $focused, 'focus has not changed');

exit_gracefully($pid);

#####################################################################

done_testing;
