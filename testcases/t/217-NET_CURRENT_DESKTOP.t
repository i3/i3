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
# Verifies the _NET_CURRENT_DESKTOP property correctly tracks the currently
# active workspace. Specifically checks that the property is 0 on startup with
# an empty config, tracks changes when switching workspaces and when
# workspaces are created and deleted.
#
# The property not being set on startup was last present in commit
# 6578976b6159437c16187cf8d1ea38aa9fec9fc8.

use i3test i3_autostart => 0;
use X11::XCB qw(PROP_MODE_REPLACE);

my $config = <<EOT;
# i3 config file (v4)
font font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $root = $x->get_root_window;
# Manually intern _NET_CURRENT_DESKTOP as $x->atom will not create atoms if
# they are not yet interned.
my $atom_cookie = $x->intern_atom(0, length("_NET_CURRENT_DESKTOP"), "_NET_CURRENT_DESKTOP");
my $_NET_CURRENT_DESKTOP = $x->intern_atom_reply($atom_cookie->{sequence})->{atom};
my $CARDINAL = $x->atom(name => 'CARDINAL')->id;

$x->delete_property($root, $_NET_CURRENT_DESKTOP);

$x->flush();

# Returns the _NET_CURRENT_DESKTOP property from the root window. This is
# the 0 based index of the current desktop.
sub current_desktop_index {
    sync_with_i3;

    my $cookie = $x->get_property(0, $root, $_NET_CURRENT_DESKTOP,
                                  $CARDINAL, 0, 1);
    my $reply = $x->get_property_reply($cookie->{sequence});

    return undef if $reply->{value_len} != 1;
    return undef if $reply->{format} != 32;
    return undef if $reply->{type} != $CARDINAL;

    return unpack 'L', $reply->{value};
}

my $pid = launch_with_config($config);

is(current_desktop_index, 0, "Starting on desktop 0");
cmd 'workspace 1';
is(current_desktop_index, 0, "Change from empty to empty");
open_window;
cmd 'workspace 0';
is(current_desktop_index, 0, "Open on 1 and view 0");
open_window;
cmd 'workspace 1';
is(current_desktop_index, 1, "Open on 0 and view 1");
cmd 'workspace 2';
is(current_desktop_index, 2, "Open and view empty");

exit_gracefully($pid);

done_testing;
