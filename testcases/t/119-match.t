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
# Tests all kinds of matching methods
#
use i3test;
use X11::XCB qw(PROP_MODE_REPLACE);

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

# Open a new window
my $window = open_window;
my $content = get_ws_content($tmp);
ok(@{$content} == 1, 'window mapped');
my $win = $content->[0];

######################################################################
# first test that matches which should not match this window really do
# not match it
######################################################################
# TODO: specify more match types
# we can match on any (non-empty) class here since that window does not have
# WM_CLASS set
cmd q|[class=".*"] kill|;
cmd q|[con_id="99999"] kill|;

is_num_children($tmp, 1, 'window still there');

# now kill the window
cmd 'nop now killing the window';
my $id = $win->{id};
cmd qq|[con_id="$id"] kill|;

wait_for_unmap $window;

cmd 'nop checking if its gone';
is_num_children($tmp, 0, 'window killed');

# TODO: same test, but with pcre expressions

######################################################################
# check that multiple criteria work are checked with a logical AND,
# not a logical OR (that is, matching is not cancelled after the first
# criterion matches).
######################################################################

$tmp = fresh_workspace;

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

my $left = open_special(name => 'left');
ok($left->mapped, 'left window mapped');

my $right = open_special(name => 'right');
ok($right->mapped, 'right window mapped');

# two windows should be here
is_num_children($tmp, 2, 'two windows opened');

cmd '[class="special" title="left"] kill';

sync_with_i3;

is_num_children($tmp, 1, 'one window still there');

######################################################################
# check that regular expressions work
######################################################################

$tmp = fresh_workspace;

$left = open_special(name => 'left', wm_class => 'special7');
ok($left->mapped, 'left window mapped');
is_num_children($tmp, 1, 'window opened');

cmd '[class="^special[0-9]$"] kill';
wait_for_unmap $left;
is_num_children($tmp, 0, 'window killed');

######################################################################
# check that UTF-8 works when matching
######################################################################

$tmp = fresh_workspace;

$left = open_special(name => 'ä 3', wm_class => 'special7');
ok($left->mapped, 'left window mapped');
is_num_children($tmp, 1, 'window opened');

cmd '[title="^\w [3]$"] kill';
wait_for_unmap $left;
is_num_children($tmp, 0, 'window killed');

done_testing;
