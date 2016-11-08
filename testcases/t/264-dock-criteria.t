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
# Verifies that command or config criteria does not match dock clients
# Bug still in: 4.12-38-ge690e3d
use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
for_window [class="dock"] move workspace current
EOT

my $pid = launch_with_config($config);

my $ws = fresh_workspace();


## command criteria should not match dock windows
open_window({
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    wm_class => "x"
});

is(get_dock_clients, 1, "created one docked client");
is_num_children($ws, 0, 'no container on the current workspace');

cmd '[class="^x$"] move workspace current';

does_i3_live
is(get_dock_clients, 1, "one docked client after move");
is_num_children($ws, 0, 'no container on the current workspace');

cmd '[class="^x$"] fullscreen';

does_i3_live
is(get_dock_clients, 1, "one docked client after fullscreen");
is_num_children($ws, 0, 'no container on the current workspace');

cmd '[class="^x$"] kill';

does_i3_live
is(get_dock_clients, 1, "one docked client after kill");
is_num_children($ws, 0, 'no container on the current workspace');


## config criteria should not match dock windows
open_window({
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
    wm_class => "dock"
});

does_i3_live
is(get_dock_clients, 2, "created second docked client");
is_num_children($ws, 0, 'no container on the current workspace');


exit_gracefully($pid);
done_testing;
