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
# Test that focusing floating windows with the command `focus [direction]`
# promotes the focused window to the top of the rendering stack.
# Ticket: #1322
# Bug still in: 4.8-88-gcc09348
use i3test;

my $ws = fresh_workspace;

my $win1 = open_floating_window;
my $win2 = open_floating_window;
my $win3 = open_floating_window;

# it's a good idea to do this a few times because of the implementation
for my $i (1 .. 3) {
    cmd 'focus left';
    my $ws_con = get_ws($ws);
    is($ws_con->{floating_nodes}[-1]->{nodes}[0]->{id}, get_focused($ws),
        "focus left put the focused window on top of the floating windows (try $i)");
}

for my $i (1 .. 3) {
    cmd 'focus right';
    my $ws_con = get_ws($ws);
    is($ws_con->{floating_nodes}[-1]->{nodes}[0]->{id}, get_focused($ws),
        "focus right put the focused window on top of the floating windows (try $i)");
}

done_testing;
