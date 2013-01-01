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
# Verifies that you can assign a window _and_ use for_window with a move
# command.
# Ticket: #909
# Bug still in: 4.4-69-g6856b23
use i3test i3_autostart => 0;
use X11::XCB qw(:all);

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

assign [instance=__i3-test-window] 2
for_window [instance=__i3-test-window] move workspace 1
EOT

my $pid = launch_with_config($config);

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

# We use dont_map because i3 will not map the window on the current
# workspace. Thus, open_window would time out in wait_for_map (2 seconds).
my $window = open_window(
    before_map => sub { set_wm_class($_->id, '__i3-test-window', '__i3-test-window') },
    dont_map => 1,
);
$window->map;

does_i3_live;

exit_gracefully($pid);

done_testing;
