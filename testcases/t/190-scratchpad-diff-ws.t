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
# Test for ticket #676: 'scratchpad show' causes a segfault if the scratchpad
# window is shown on another workspace.
#
use i3test;

my $i3 = i3(get_socket_path());
my $tmp = fresh_workspace;

my $win = open_window;

my $scratch = open_window(wm_class => 'special');
cmd '[class="special"] move scratchpad';

is_num_children($tmp, 1, 'one window on current ws');

my $otmp = fresh_workspace;
cmd 'scratchpad show';

cmd "workspace $tmp";
cmd '[class="special"] scratchpad show';

does_i3_live;

done_testing;
