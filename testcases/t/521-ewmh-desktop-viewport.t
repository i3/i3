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
# Test that the EWMH specified property _NET_DESKTOP_VIEWPORT is updated
# properly on the root window. We interpret this as a list of x/y coordinate
# pairs for the upper left corner of the respective outputs of the workspaces
# Ticket: #1241
use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

workspace 0 output fake-0
workspace 1 output fake-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

my $pid = launch_with_config($config);

sub get_desktop_viewport {
    # Make sure that i3 pushed its changes to X11 before querying.
    sync_with_i3;

    my $cookie = $x->get_property(
        0,
        $x->get_root_window(),
        $x->atom(name => '_NET_DESKTOP_VIEWPORT')->id,
        $x->atom(name => 'CARDINAL')->id,
        0,
        4096
    );

    my $reply = $x->get_property_reply($cookie->{sequence});

    return 0 if $reply->{value_len} == 0;

    my $len = $reply->{length};

    return unpack ("L$len", $reply->{value});
}

# initialize the workspaces
cmd 'workspace 1';
cmd 'workspace 0';

my @expected_viewport = (0, 0, 1024, 0);
my @desktop_viewport = get_desktop_viewport;

is_deeply(\@desktop_viewport, \@expected_viewport,
    '_NET_DESKTOP_VIEWPORT should be an array of x/y coordinate pairs for the upper left corner of the respective outputs of the workspaces');

cmd 'workspace 0';
open_window;
cmd 'workspace 3';

@expected_viewport = (0, 0, 0, 0, 1024, 0);
@desktop_viewport = get_desktop_viewport;

is_deeply(\@desktop_viewport, \@expected_viewport,
    'it should be updated when a new workspace appears');

cmd 'rename workspace 3 to 2';

@expected_viewport = (0, 0, 0, 0, 1024, 0);
@desktop_viewport = get_desktop_viewport;

is_deeply(\@desktop_viewport, \@expected_viewport,
    'it should stay up to date when a workspace is renamed');

cmd 'workspace 0';

@expected_viewport = (0, 0, 1024, 0);
@desktop_viewport = get_desktop_viewport;

is_deeply(\@desktop_viewport, \@expected_viewport,
    'it should be updated when a workspace is emptied');

exit_gracefully($pid);

done_testing;
