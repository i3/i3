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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Verifies the _NET_WORKAREA hint is deleted in case it is already set on the
# root window.
# Ticket: #1038
# Bug still in: 4.5.1-103-g1f8a860
use i3test i3_autostart => 0;
use X11::XCB qw(PROP_MODE_REPLACE);

my $atom_cookie = $x->intern_atom(
    0, # create!
    length('_NET_WORKAREA'),
    '_NET_WORKAREA',
);

my $_net_workarea_id = $x->intern_atom_reply($atom_cookie->{sequence})->{atom};

$x->change_property(
    PROP_MODE_REPLACE,
    $x->get_root_window(),
    $_net_workarea_id,
    $x->atom(name => 'CARDINAL')->id,
    32,
    4,
    pack('L4', 0, 0, 1024, 768));
$x->flush;

sub is_net_workarea_set {
    my $cookie = $x->get_property(
        0,
        $x->get_root_window(),
        $x->atom(name => '_NET_WORKAREA')->id,
        $x->atom(name => 'CARDINAL')->id,
        0,
        4096,
    );
    my $reply = $x->get_property_reply($cookie->{sequence});
    return 0 if $reply->{value_len} == 0;
    return 0 if $reply->{format} == 0;
    return 1
}

ok(is_net_workarea_set(), '_NET_WORKAREA is set before starting i3');

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
fake-outputs 1024x768+0+0
EOT

my $pid = launch_with_config($config);

ok(!is_net_workarea_set(), '_NET_WORKAREA not set after starting i3');

exit_gracefully($pid);

done_testing;
