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
# Test that the EWMH specified property _NET_DESKTOP_NAMES is updated properly
# on the root window. We interpret this as a list of the open workspace names.
# Ticket: #1241
use i3test;

sub get_desktop_names {
    # Make sure that i3 pushed its changes to X11 before querying.
    sync_with_i3;

    my $cookie = $x->get_property(
        0,
        $x->get_root_window(),
        $x->atom(name => '_NET_DESKTOP_NAMES')->id,
        $x->atom(name => 'UTF8_STRING')->id,
        0,
        4096,
    );

    my $reply = $x->get_property_reply($cookie->{sequence});

    return 0 if $reply->{value_len} == 0;

    # the property is a null-delimited list of utf8 strings ;;
    return split /\0/, $reply->{value};
}

cmd 'workspace foo';

my @expected_names = ('foo');
my @desktop_names = get_desktop_names;

is_deeply(\@desktop_names, \@expected_names, '_NET_DESKTOP_NAMES should be an array of the workspace names');

# open a new workspace and see that the property is updated correctly
open_window;
cmd 'workspace bar';

@desktop_names = get_desktop_names;
@expected_names = ('foo', 'bar');

is_deeply(\@desktop_names, \@expected_names, 'it should be updated when a new workspace appears');

# rename the workspace and see that the property is updated correctly
cmd 'rename workspace bar to baz';

@desktop_names = get_desktop_names;
@expected_names = ('foo', 'baz');

is_deeply(\@desktop_names, \@expected_names, 'it should be updated when a workspace is renamed');

# empty a workspace and see that the property is updated correctly
cmd 'workspace foo';

@desktop_names = get_desktop_names;
@expected_names = ('foo');

is_deeply(\@desktop_names, \@expected_names, 'it should be updated when a workspace is emptied');

done_testing;
