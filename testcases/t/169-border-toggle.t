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
is($nodes[0]->{border}, 'pixel', 'border style pixel');
is($nodes[0]->{current_border_width}, 1, 'border width = 1px');

cmd 'border none';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'none', 'border style none');
is($nodes[0]->{current_border_width}, 0, 'border width = 0px');

cmd 'border normal';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'normal', 'border style back to normal');
is($nodes[0]->{current_border_width}, 2, 'border width = 2px');

cmd 'border toggle';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'none', 'border style none');
is($nodes[0]->{current_border_width}, 0, 'border width = 0px');

cmd 'border toggle';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'pixel', 'border style pixel');
is($nodes[0]->{current_border_width}, 1, 'border width = 1px');

cmd 'border toggle';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'normal', 'border style back to normal');
is($nodes[0]->{current_border_width}, 2, 'border width = 2px');

cmd 'border toggle 10';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'none', 'border style back to none even with width argument');
is($nodes[0]->{current_border_width}, 0, 'border width = 0px');

cmd 'border toggle 10';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'pixel', 'border style pixel');
is($nodes[0]->{current_border_width}, 10, 'border width = 10px');

cmd 'border toggle 10';
@nodes = @{get_ws_content($tmp)};
is($nodes[0]->{border}, 'normal', 'border style back to normal');
is($nodes[0]->{current_border_width}, 10, 'border width = 10px');

done_testing;
