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
# Make sure floating containers really can't be split.
# Ticket: #1177
# Bug still in: 4.7.2-81-g905440d
use i3test;

my $ws = fresh_workspace;
my $window = open_floating_window;
cmd "layout stacked";
cmd "splitv";

my $floating_con = get_ws($ws)->{floating_nodes}[0]->{nodes}[0];

is(@{$floating_con->{nodes}}, 0, 'floating con is still a leaf');

cmd 'floating disable';

does_i3_live;

done_testing;
