#!perl
# vim:ts=4:sw=4:expandtab
# Check if numbered workspaces and named workspaces are sorted in the right way
# in get_workspaces IPC output (necessary for i3bar etc.).
use i3test;

my $i3 = i3(get_socket_path());

sub check_order {
    my ($msg) = @_;

    my @ws = @{$i3->get_workspaces->recv};
    my @nums = map { $_->{num} } grep { defined($_->{num}) } @ws;
    my @sorted = sort @nums;

    cmp_deeply(\@nums, \@sorted, $msg);
}

check_order('workspace order alright before testing');

#############################################################################
# open a window to keep this ws open
#############################################################################

cmd "workspace 93";

open_window;

my @ws = @{$i3->get_workspaces->recv};
my @f = grep { defined($_->{num}) && $_->{num} == 93 } @ws;
is(@f, 1, 'ws 93 found by num');
check_order('workspace order alright after opening 93');

cmd "workspace 92";
open_window;
check_order('workspace order alright after opening 92');

cmd "workspace 94";
open_window;
check_order('workspace order alright after opening 94');

cmd "workspace 96";
open_window;
check_order('workspace order alright after opening 96');

cmd "workspace foo";
open_window;
check_order('workspace order alright after opening foo');

cmd "workspace 91";
open_window;
check_order('workspace order alright after opening 91');

done_testing;
