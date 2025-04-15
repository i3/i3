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
# Test that changes to WM_CLASS are internally processed by i3 by updating the
# cached property and running assignments. This allows the property to be used
# in criteria selection
# Ticket: #1052
# Bug still in: 4.8-73-g6bf7f8e
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
for_window [class="Special"] mark special_class_mark
EOT
use X11::XCB qw(PROP_MODE_REPLACE);

sub change_window_class {
    my ($window, $class, $length) = @_;
    my $atomname = $x->atom(name => 'WM_CLASS');
    my $atomtype = $x->atom(name => 'STRING');
    $length ||= length($class) + 1;
    $x->change_property(
        PROP_MODE_REPLACE,
        $window->id,
        $atomname->id,
        $atomtype->id,
        8,
        $length,
        $class
    );
    sync_with_i3;
}

my $ws = fresh_workspace;

my $win = open_window;

change_window_class($win, "special\0Special");

my $con = @{get_ws_content($ws)}[0];

is($con->{window_properties}->{class}, 'Special',
    'The container class should be updated when a window changes class');

is($con->{window_properties}->{instance}, 'special',
    'The container instance should be updated when a window changes instance');

# The mark `special_class_mark` is added in a `for_window` assignment in the
# config for testing purposes
is_deeply($con->{marks}, [ 'special_class_mark' ],
    'A `for_window` assignment should run for a match when the window changes class');

change_window_class($win, "abcdefghijklmnopqrstuv\0abcd", 24);

$con = @{get_ws_content($ws)}[0];

is($con->{window_properties}->{class}, 'a',
    'Non-null-terminated strings should be handled correctly');

done_testing;
