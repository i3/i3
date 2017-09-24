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
# Check if stacking containers can be used independently of
# the split mode (horizontal/vertical) of the underlying
# container.
#
use i3test;

my $i3 = i3(get_socket_path());

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

# Enforce vertical split mode
cmd 'split v';

my $first = open_empty_con($i3);
my $second = open_empty_con($i3);

isnt($first, $second, 'two different containers opened');

##############################################################
# change mode to stacking and cycle through the containers
##############################################################

cmd 'layout stacking';
is(get_focused($tmp), $second, 'second container still focused');

cmd 'focus down';
is(get_focused($tmp), $first, 'first container focused');

cmd 'focus up';
is(get_focused($tmp), $second, 'second container focused again');

##############################################################
# now change the orientation to horizontal and cycle
##############################################################

cmd 'focus parent';
cmd 'split h';
cmd 'focus child';

cmd 'focus down';
is(get_focused($tmp), $first, 'first container focused');

cmd 'focus up';
is(get_focused($tmp), $second, 'second container focused again');


done_testing;
