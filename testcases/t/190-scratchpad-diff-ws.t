#!perl
# vim:ts=4:sw=4:expandtab
#
# Test for ticket #676: 'scratchpad show' causes a segfault if the scratchpad
# window is shown on another workspace.
#
use i3test;
use List::Util qw(first);
use X11::XCB qw(:all);

my $i3 = i3(get_socket_path());
my $tmp = fresh_workspace;

# TODO: move to X11::XCB
sub set_wm_class {
    my ($id, $class, $instance) = @_;

    # Add a _NET_WM_STRUT_PARTIAL hint
    my $atomname = $x->atom(name => 'WM_CLASS');
    my $atomtype = $x->atom(name => 'STRING');

    $x->change_property(
        PROP_MODE_REPLACE,
        $id,
        $atomname->id,
        $atomtype->id,
        8,
        length($class) + length($instance) + 2,
        "$instance\x00$class\x00"
    );
}

sub open_special {
    my %args = @_;
    my $wm_class = delete($args{wm_class}) || 'special';

    return open_window(
        %args,
        before_map => sub { set_wm_class($_->id, $wm_class, $wm_class) },
    );
}

my $win = open_window;

my $scratch = open_special;
cmd '[class="special"] move scratchpad';

my ($nodes, $focus) = get_ws_content($tmp);
is(scalar @$nodes, 1, 'one window on current ws');

my $otmp = fresh_workspace;
cmd 'scratchpad show';

cmd "workspace $tmp";
cmd '[class="special"] scratchpad show';

does_i3_live;

done_testing;
