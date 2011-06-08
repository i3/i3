#!perl
# vim:ts=4:sw=4:expandtab
#
# Check if stacking containers can be used independantly of
# the split mode (horizontal/vertical) of the underlying
# container.
#
use i3test;

my $i3 = i3("/tmp/nestedcons");

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

cmd 'level up';
cmd 'split h';
cmd 'level down';

cmd 'focus down';
is(get_focused($tmp), $first, 'first container focused');

cmd 'focus up';
is(get_focused($tmp), $second, 'second container focused again');


done_testing;
