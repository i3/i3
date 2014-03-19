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
# Test that i3 does not crash when a command is issued that would resize a dock
# client.
# Ticket: #1201
# Bug still in: 4.7.2-107-g9b03be6
use i3test i3_autostart => 0;

my $config = <<'EOT';
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $pid = launch_with_config($config);

my $i3 = i3(get_socket_path());
$i3->connect()->recv;

my $window = open_window(
    wm_class => 'special',
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_DOCK'),
);

cmd('[class="special"] resize grow height 160 px or 16 ppt');

does_i3_live;

exit_gracefully($pid);

done_testing;
