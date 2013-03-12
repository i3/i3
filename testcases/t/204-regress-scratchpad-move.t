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
# Moves the last window of a workspace to the scratchpad. The workspace will be
# cleaned up and previously, the subsequent focusing of a destroyed container
# would crash i3.
# Ticket: #913
# Bug still in: 4.4-97-gf767ac3
use i3test;
use X11::XCB qw(:all);

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

my $tmp = fresh_workspace;

# Open a new window which we can identify later on based on its WM_CLASS.
my $scratch = open_special;

my $tmp2 = fresh_workspace;

cmd '[class="special"] move scratchpad';

does_i3_live;

done_testing;
