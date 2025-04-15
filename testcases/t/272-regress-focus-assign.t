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
# Regression: Checks if focus is stolen when a window is managed which is
# assigned to an invisible workspace
#
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
assign [class="special"] targetws
EOT

sub open_special {
    my %args = @_;
    $args{name} //= 'special window';

    # We use dont_map because i3 will not map the window on the current
    # workspace. Thus, open_window would time out in wait_for_map (2 seconds).
    my $window = open_window(
        %args,
        wm_class => 'special',
        dont_map => 1,
    );
    $window->map;
    return $window;
}

#####################################################################
# start a window and see that it does not get assigned with an empty config
#####################################################################

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');
ok(get_ws($tmp)->{focused}, 'current workspace focused');

my $window = open_special;
sync_with_i3;

ok(@{get_ws_content($tmp)} == 0, 'special window not on current workspace');
ok(@{get_ws_content('targetws')} == 1, 'special window on targetws');
ok(get_ws($tmp)->{focused}, 'current workspace still focused');

#####################################################################
# the same test, but with a floating window
#####################################################################

$window = open_special(
    window_type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
);

ok(@{get_ws_content($tmp)} == 0, 'special window not on current workspace');
ok(@{get_ws_content('targetws')} == 1, 'special window on targetws');
ok(get_ws($tmp)->{focused}, 'current workspace still focused');

$window->destroy;

done_testing;
