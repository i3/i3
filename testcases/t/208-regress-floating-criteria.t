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
# Regression test for focus handling when using floating enable/disable with
# criteria for windows on non-visible workspaces.
# Ticket: #1027
# Bug still in: 4.5.1-90-g6582da9
use i3test i3_autostart => 0;
use X11::XCB qw(PROP_MODE_REPLACE);

my $config = <<'EOT';
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

assign [class="^special$"] → mail
for_window [class="^special$"] floating enable, floating disable
EOT

my $pid = launch_with_config($config);

# TODO: move this to X11::XCB::Window
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

my $window = open_window(
    name => 'Borderless window',
    before_map => sub { set_wm_class($_->id, 'special', 'special') },
    dont_map => 1,
);
$window->map;

sync_with_i3;

cmd '[class="^special$"] focus';

does_i3_live;

exit_gracefully($pid);

done_testing;
