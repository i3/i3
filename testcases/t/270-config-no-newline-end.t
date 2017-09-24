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
# Make sure that configs that end without a newline don't crash i3.
# Ticket: #2934
use i3test i3_autostart => 0;

my $first_lines = <<'EOT';
set $workspace1 workspace number 1
set $workspace0 workspace eggs

bindsym Mod4+1 $workspace1
EOT

# Intentionally don't add a trailing newline for the last line since this is
# what triggered the bug.
my $last_line = 'bindsym Mod4+0 $workspace0';
my $config = "${first_lines}${last_line}";

my $pid = launch_with_config($config);
does_i3_live;

my $i3 = i3(get_socket_path());
my $ws = $i3->get_workspaces->recv;
is($ws->[0]->{name}, 'eggs', 'last line processed correctly');

exit_gracefully($pid);
done_testing;
