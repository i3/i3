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
# Tests whether opening an empty container and killing it again works
#
use List::Util qw(first);
use i3test;

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

# Open a new container
cmd 'open';

ok(@{get_ws_content($tmp)} == 1, 'container opened');

cmd 'kill';
ok(@{get_ws_content($tmp)} == 0, 'container killed');

##############################################################
# open two containers and kill the one which is not focused
# by its ID to test if the parser correctly matches the window
##############################################################

cmd 'open';
cmd 'open';
ok(@{get_ws_content($tmp)} == 2, 'two containers opened');

my $content = get_ws_content($tmp);
my $not_focused = first { !$_->{focused} } @{$content};
my $id = $not_focused->{id};

cmd "[con_id=\"$id\"] kill";

$content = get_ws_content($tmp);
ok(@{$content} == 1, 'one container killed');
ok($content->[0]->{id} != $id, 'correct window killed');

done_testing;
