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
# Verifies that i3 does not crash when opening a floating sticky on one output
# and then switching empty workspaces on the other output.
# Ticket: #3075
# Bug still in: 4.14-191-g9d2d602d
use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

# A window on the left output.
fresh_workspace(output => 0);
open_window;
cmd 'sticky enable, floating enable';

# Switch to the right output and open a new workspace.
my $ws = fresh_workspace(output => 1);
does_i3_live;

# Verify results.
is(@{get_ws($ws)->{floating_nodes}}, 0, 'workspace in right output is empty');
$ws = fresh_workspace(output => 0);
is(@{get_ws($ws)->{floating_nodes}}, 1, 'new workspace in left output has the sticky container');

done_testing;
