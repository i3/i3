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
# Tests if the 'border toggle' command works correctly
#
use i3test;

my $tmp = fresh_workspace;

cmd 'open';

my @nodes = @{get_ws_content($tmp)};
is(@nodes, 1, 'one container on this workspace');
is($nodes[0]->{border}, 'normal', 'border style normal');

cmd 'border 1pixel';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, '1pixel', 'border style 1pixel');

cmd 'border none';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'none', 'border style none');

cmd 'border normal';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'normal', 'border style back to normal');

cmd 'border toggle';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'none', 'border style none');

cmd 'border toggle';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, '1pixel', 'border style 1pixel');

cmd 'border toggle';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'normal', 'border style back to normal');

done_testing;
