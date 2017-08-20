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
# Verifies i3 doesn’t warp when a new floating window is opened under the cursor
# over an unfocused workspace.
# Ticket: #2681
# Bug still in: 4.13-210-g80c23afa
use i3test i3_autostart => 0;

# Ensure the pointer is at (0, 0) so that we really start on the first
# (the left) workspace.
$x->root->warp_pointer(0, 0);

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0

focus_follows_mouse no
EOT

my $pid = launch_with_config($config);

cmd 'focus output fake-0';
my $s0_ws = fresh_workspace;

cmd 'focus output fake-1';
my $s1_ws = fresh_workspace;
open_window;

# Move mouse to fake-0
sync_with_i3;
$x->root->warp_pointer(500, 0);
sync_with_i3;

my $dropdown = open_floating_window;
$dropdown->rect(X11::XCB::Rect->new(x => 1, y => 1, width => 100, height => 100));
sync_with_i3;

my $cookie = $x->query_pointer($dropdown->{id});
my $reply = $x->query_pointer_reply($cookie->{sequence});
cmp_ok($reply->{root_x}, '<', 1024, 'pointer still on fake-0');
cmp_ok($reply->{root_y}, '<', 768, 'pointer still on fake-0');

exit_gracefully($pid);

done_testing;
