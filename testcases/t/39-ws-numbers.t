#!perl
# vim:ts=4:sw=4:expandtab
# Check if numbered workspaces and named workspaces are sorted in the right way
# in get_workspaces IPC output (necessary for i3bar etc.).
use i3test tests => 9;
use X11::XCB qw(:all);
use Time::HiRes qw(sleep);

BEGIN {
    use_ok('X11::XCB::Window');
}

my $i3 = i3("/tmp/nestedcons");
my $x = X11::XCB::Connection->new;

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

$i3->command("workspace 93")->recv;

open_standard_window($x);

my @ws = @{$i3->get_workspaces->recv};
my @f = grep { defined($_->{num}) && $_->{num} == 93 } @ws;
is(@f, 1, 'ws 93 found by num');
check_order('workspace order alright after opening 93');

$i3->command("workspace 92")->recv;
open_standard_window($x);
check_order('workspace order alright after opening 92');

$i3->command("workspace 94")->recv;
open_standard_window($x);
check_order('workspace order alright after opening 94');

$i3->command("workspace 96")->recv;
open_standard_window($x);
check_order('workspace order alright after opening 96');

$i3->command("workspace foo")->recv;
open_standard_window($x);
check_order('workspace order alright after opening foo');

$i3->command("workspace 91")->recv;
open_standard_window($x);
check_order('workspace order alright after opening 91');
