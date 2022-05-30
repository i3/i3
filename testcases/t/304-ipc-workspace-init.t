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
# Test that the workspace init event is correctly sent.
# Ticket: #3631
# Bug still in: 4.16-85-g2d6e09a6

use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

# fake-1 under fake-0 to not interfere with left/right wrapping
fake-outputs 1024x768+0+0,1024x768+0+1024
workspace X output fake-1
EOT

sub workspace_init_subtest {
    my $cmd = shift;
    my $num_events = shift;
    my @events = events_for(sub { cmd $cmd }, 'workspace');

    my @init = grep { $_->{change} eq 'init' } @events;
    my $len = scalar @init;
    is($len, $num_events, "Received $num_events workspace::init event");
    $num_events = $len if $len < $num_events;
    for my $idx (0 .. $num_events - 1) {
        my $name = shift;
        my $output = shift;
        is($init[$idx]->{current}->{name}, $name, "workspace name $name matches");
        is($init[$idx]->{current}->{output}, $output, "workspace output $output matches");
    }
}

subtest 'focus outputs', \&workspace_init_subtest, 'focus output fake-1, focus output fake-0', 0;
subtest 'new workspaces', \&workspace_init_subtest,
  'workspace a, workspace b, workspace a, workspace a', 3, 'a',
  'fake-0', 'b', 'fake-0', 'a', 'fake-0';
open_window;    # Prevent workspace "a" from being deleted.
subtest 'return on existing workspace', \&workspace_init_subtest,
  'workspace a, workspace b, workspace a', 1, 'b', 'fake-0';
subtest 'assigned workspace is already open', \&workspace_init_subtest,
  'workspace X, workspace b, workspace a', 1, 'b', 'fake-1';
subtest 'assigned workspace was deleted and now is initialized again', \&workspace_init_subtest,
  'workspace X, workspace b, workspace a', 2, 'X', 'fake-1', 'b', 'fake-1';
subtest 'move workspace to output', \&workspace_init_subtest,
  'move workspace to output fake-1, move workspace to output fake-0', 2, '1', 'fake-0', 'X',
  'fake-1';
subtest 'move window to workspace', \&workspace_init_subtest, 'move to workspace b', 1, 'b',
  'fake-0';
subtest 'back_and_forth', \&workspace_init_subtest,
  'workspace b, workspace back_and_forth, workspace b', 1,
  'a', 'fake-0';
subtest 'move window to workspace back_and_forth', \&workspace_init_subtest,
  'move window to workspace back_and_forth', 1, 'a', 'fake-0';

done_testing;
