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
# Tests whether moving workspaces between outputs works correctly.
use i3test i3_autostart => 0;
use List::Util qw(first);

# Ensure the pointer is at (0, 0) so that we really start on the first
# (the left) workspace.
$x->root->warp_pointer(0, 0);

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

my $pid = launch_with_config($config);

sub workspaces_per_screen {
    my $i3 = i3(get_socket_path());
    my $tree = $i3->get_tree->recv;
    my @outputs = @{$tree->{nodes}};

    my $fake0 = first { $_->{name} eq 'fake-0' } @outputs;
    my $fake0_content = first { $_->{type} == 2 } @{$fake0->{nodes}};

    my $fake1 = first { $_->{name} eq 'fake-1' } @outputs;
    my $fake1_content = first { $_->{type} == 2 } @{$fake1->{nodes}};

    my @fake0_workspaces = map { $_->{name} } @{$fake0_content->{nodes}};
    my @fake1_workspaces = map { $_->{name} } @{$fake1_content->{nodes}};

    return \@fake0_workspaces, \@fake1_workspaces;
}

# Switch to temporary workspaces on both outputs so the numbers are free.
my $tmp_right = fresh_workspace(output => 1);
my $tmp_left = fresh_workspace(output => 0);

cmd 'workspace 1';
# Keep that workspace open.
my $win1 = open_window;

cmd 'workspace 5';
# Keep that workspace open.
open_window;

cmd "workspace $tmp_right";
cmd 'workspace 2';
# Keep that workspace open.
open_window;

my ($x0, $x1) = workspaces_per_screen();
is_deeply($x0, [ '1', '5' ], 'workspace 1 and 5 on fake-0');
is_deeply($x1, [ '2' ], 'workspace 2 on fake-1');

cmd 'workspace 1';

my ($nodes, $focus) = get_ws_content('1');
is($nodes->[0]->{window}, $win1->id, 'window 1 on workspace 1');

cmd 'move workspace next';

($nodes, $focus) = get_ws_content('2');
is($nodes->[1]->{window}, $win1->id, 'window 1 on workspace 2 after moving');

cmd 'move workspace prev';

($nodes, $focus) = get_ws_content('1');
is($nodes->[0]->{window}, $win1->id, 'window 1 on workspace 1');

cmd 'move workspace next_on_output';

($nodes, $focus) = get_ws_content('5');
is($nodes->[1]->{window}, $win1->id, 'window 1 on workspace 5 after moving');

exit_gracefully($pid);

done_testing;
