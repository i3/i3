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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Tests that a workspace keeps its focus after moving even in event-heavy sessions.
# Ticket: #6372
# Bug still in: 4.25-24-gf7d5b898
use i3test i3_autostart => 0;

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

my $pid = launch_with_config($config);

cmd 'workspace ws1';
cmd 'layout default';
cmd 'split v';

my $top = open_window;
my $bottom = open_window;
is($x->input_focus, $bottom->id, 'bottom window focused');

for (1 .. 1000) {
    cmd 'move up; move down';
}
is($x->input_focus, $bottom->id, 'bottom window still focused after some events');

cmd 'move workspace to output right';
is($x->input_focus, $bottom->id, 'bottom window still focused after some events and after workspace has been moved');
cmd 'move workspace to output left';

for (1 .. 9000) {
    cmd 'move up; move down';
}
is($x->input_focus, $bottom->id, 'bottom window still focused after many events');

cmd 'move workspace to output right';
is($x->input_focus, $bottom->id, 'bottom window still focused after many events and after workspace has been moved');

exit_gracefully($pid);

done_testing;
