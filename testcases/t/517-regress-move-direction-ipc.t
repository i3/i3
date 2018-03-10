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
# Make sure the command `move <direction>` properly sends the workspace focus
# ipc event required for i3bar to be properly updated and redrawn.
#
# Bug still in: 4.6-195-g34232b8
use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
workspace ws-left output fake-0
workspace ws-right output fake-1
EOT

# open two windows on the left output
cmd 'workspace ws-left';
open_window;
open_window;

sub focus_subtest {
    my ($cmd, $want) = @_;

    my @events = events_for(
	sub { cmd $cmd },
	'workspace');

    my @focus = grep { $_->{change} eq 'focus' } @events;
    is(scalar @focus, 1, 'Received 1 workspace::focus event');
    is($focus[0]->{current}->{name}, 'ws-right', 'focus event gave the right workspace');
    is(@{$focus[0]->{current}->{nodes}}, $want, 'focus event gave the right number of windows on the workspace');
}

# move a window over to the right output
subtest 'move right (1)', \&focus_subtest, 'move right', 1;

# move another window
cmd 'workspace ws-left';
subtest 'move right (2)', \&focus_subtest, 'move right', 2;

done_testing;
