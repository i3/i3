#!perl
# vim:ts=4:sw=4:expandtab
# Regression: Check if the focus stays the same when switching the layout
# bug introduced by 77d0d42ed2d7ac8cafe267c92b35a81c1b9491eb
use i3test;

my $i3 = i3(get_socket_path());

sub check_order {
    my ($msg) = @_;

    my @ws = @{$i3->get_workspaces->recv};
    my @nums = map { $_->{num} } grep { defined($_->{num}) } @ws;
    my @sorted = sort @nums;

    cmp_deeply(\@nums, \@sorted, $msg);
}

my $tmp = fresh_workspace;

my $left = open_window;
my $mid = open_window;
my $right = open_window;

sync_with_i3($x);

diag("left = " . $left->id . ", mid = " . $mid->id . ", right = " . $right->id);

is($x->input_focus, $right->id, 'Right window focused');

cmd 'focus left';

is($x->input_focus, $mid->id, 'Mid window focused');

cmd 'layout stacked';

is($x->input_focus, $mid->id, 'Mid window focused');

done_testing;
