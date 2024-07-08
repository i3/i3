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
# Verifies that windows are properly recognized as urgent when they start up
# with the urgency hint already set (and are assigned to a non-visible
# workspace).
#
# Ticket: #1086
# Bug still in: 4.6-62-g7098ef6
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

assign [class="special"] nonvisible
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
    $window->add_hint('urgency');
    $window->map;
    return $window;
}

my $tmp = fresh_workspace;

ok((scalar grep { $_ eq 'nonvisible' } @{get_workspace_names()}) == 0,
   'assignment destination workspace does not exist yet');

my $window = open_special;
sync_with_i3;

ok((scalar grep { $_ eq 'nonvisible' } @{get_workspace_names()}) > 0,
   'assignment destination workspace exists');

my @urgent = grep { $_->{urgent} } @{get_ws_content('nonvisible')};
isnt(@urgent, 0, 'urgent window(s) found on destination workspace');

done_testing;
