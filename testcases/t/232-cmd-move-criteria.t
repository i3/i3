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
# Test that the `move [direction]` command works with criteria
# Bug still in: 4.8-16-g6888a1f
use i3test;

my $ws = fresh_workspace;

my $win1 = open_window;
my $win2 = open_window;
my $win3 = open_window;

# move win1 from the left to the right
cmd '[id="' . $win1->{id} . '"] move right';

# now they should be switched, with win2 still being focused
my $ws_con = get_ws($ws);

# win2 should be on the left
is($ws_con->{nodes}[0]->{window}, $win2->{id}, 'the `move [direction]` command should work with criteria');
is($x->input_focus, $win3->{id}, 'it should not disturb focus');

done_testing;
