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
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Verifies _NET_DESKTOP_NAMES, _NET_CURRENT_DESKTOP and _NET_CURRENT_DESKTOP
# are updated properly when closing an inactive workspace container.
# See github issue #3126

use i3test;

sub get_desktop_names {
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

sub get_num_of_desktops {
    sync_with_i3;

    my $cookie = $x->get_property(
        0,
        $x->get_root_window(),
        $x->atom(name => '_NET_NUMBER_OF_DESKTOPS')->id,
        $x->atom(name => 'CARDINAL')->id,
        0,
        4,
    );

    my $reply = $x->get_property_reply($cookie->{sequence});

    return undef if $reply->{value_len} != 1;
    return undef if $reply->{format} != 32;
    return undef if $reply->{type} != $x->atom(name => 'CARDINAL')->id,;

    return unpack 'L', $reply->{value};
}

sub get_current_desktop {
    sync_with_i3;

    my $cookie = $x->get_property(
        0,
        $x->get_root_window(),
        $x->atom(name => '_NET_CURRENT_DESKTOP')->id,
        $x->atom(name => 'CARDINAL')->id,
        0,
        4,
    );

    my $reply = $x->get_property_reply($cookie->{sequence});

    return undef if $reply->{value_len} != 1;
    return undef if $reply->{format} != 32;
    return undef if $reply->{type} != $x->atom(name => 'CARDINAL')->id,;

    return unpack 'L', $reply->{value};
}

cmd 'workspace 0';
my $first = open_window;

cmd 'workspace 1';
my $second = open_window;

cmd 'workspace 2';
my $third = open_window;

# Sanity check
is(get_current_desktop, 2);
is(get_num_of_desktops, 3);
my @actual_names = get_desktop_names;
my @expected_names = ('0', '1', '2');
is_deeply(\@actual_names, \@expected_names);

# Kill first window to close a workspace.
cmd '[id="' . $second->id . '"] kill';

is(get_current_desktop, 2, '_NET_CURRENT_DESKTOP should be updated');
is(get_num_of_desktops, 2, '_NET_NUMBER_OF_DESKTOPS should be updated');
my @actual_names = get_desktop_names;
my @expected_names = ('0', '2');
is_deeply(\@actual_names, \@expected_names, '_NET_DESKTOP_NAMES should be updated');


done_testing;
