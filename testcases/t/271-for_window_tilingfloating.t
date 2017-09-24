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
use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [tiling] mark tiled
for_window [floating] mark floated
EOT
use X11::XCB qw(PROP_MODE_REPLACE);

##############################################################
# 13: check that the tiling / floating criteria work.
##############################################################

my $tmp = fresh_workspace;

open_window;
open_floating_window;

my @nodes = @{get_ws($tmp)->{nodes}};
cmp_ok(@nodes, '==', 1, 'one tiling container on this workspace');
is_deeply($nodes[0]->{marks}, [ 'tiled' ], "mark set for 'tiling' criterion");

@nodes = @{get_ws($tmp)->{floating_nodes}};
cmp_ok(@nodes, '==', 1, 'one floating container on this workspace');
is_deeply($nodes[0]->{nodes}[0]->{marks}, [ 'floated' ], "mark set for 'floating' criterion");

##############################################################

done_testing;
