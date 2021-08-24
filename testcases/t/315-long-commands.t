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
# Test that commands with more than 10 non-identified words doesn't works
# 10 is the magic number chosen for the stack size which is why it's used here
# Ticket: #2968
# Bug still in: 4.19.2-103-gfc65ca36
use i3test;

######################################################################
# 1) run a long command
######################################################################

my $i3 = i3(get_socket_path());
my $tmp = fresh_workspace;

my $floatwin = open_floating_window;


my ($absolute_before, $top_before) = $floatwin->rect;

cmd 'move window container to window container to window container to left';

sync_with_i3;

my ($absolute, $top) = $floatwin->rect;

is($absolute->x, ($absolute_before->x - 10), 'moved 10 px to the left');
is($absolute->y, $absolute_before->y, 'y not changed');
is($absolute->width, $absolute_before->width, 'width not changed');
is($absolute->height, $absolute_before->height, 'height not changed');

done_testing;
