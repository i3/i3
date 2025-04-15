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
# Tests focus_wrapping yes|no|force|workspace with cmp_tree
# Tickets: #2180 #2352
use i3test i3_autostart => 0;

my $pid = 0;

sub focus_wrapping {
    my ($setting) = @_;

    print "--------------------------------------------------------------------------------\n";
    print "                             focus_wrapping $setting\n";
    print "--------------------------------------------------------------------------------\n";
    exit_gracefully($pid) if $pid > 0;

    my $config = <<"EOT";
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0,1024x768+1024+768,1024x768+0+768
workspace left-top output fake-0
workspace right-top output fake-1
workspace right-bottom output fake-2
workspace left-bottom output fake-3

focus_wrapping $setting
EOT
    $pid = launch_with_config($config);
}

###############################################################################
focus_wrapping('yes');
###############################################################################

cmp_tree(
    msg => 'Normal focus up - should work for all options',
    layout_before => 'S[a b*] V[c d T[e f g]]',
    layout_after => 'S[a* b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus up';
    });
cmp_tree(
    msg => 'Normal focus right - should work for all options',
    layout_before => 'S[a b] V[c d T[e* f g]]',
    layout_after => 'S[a b] V[c d T[e f* g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus right';
    });
cmp_tree(
    msg => 'Focus leaves workspace vertically',
    layout_before => 'S[a b*] V[c d T[e f g]]',
    layout_after => 'S[a b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus down';
        is(focused_ws, 'left-bottom', 'Correct workspace focused');
    });
cmp_tree(
    msg => 'Focus wraps vertically',
    layout_before => 'S[a* b] V[c d T[e f g]]',
    layout_after => 'S[a b*] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus up';
    });
cmp_tree(
    msg => 'Focus wraps horizontally',
    layout_before => 'S[a b*] V[c d T[e f g]]',
    layout_after => 'S[a b] V[c d T[e f g*]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus left';
    });
cmp_tree(
    msg => 'Directional focus in the orientation of the parent does not wrap',
    layout_before => 'S[a b] V[c d T[e* f g]]',
    layout_after => 'S[a b*] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus left';
    });
cmp_tree(
    msg => 'Focus leaves workspace horizontally',
    layout_before => 'S[a b] V[c d* T[e f g*]]',
    layout_after => 'S[a b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus right';
        is(focused_ws, 'right-top', 'Correct workspace focused');
    });

###############################################################################
focus_wrapping('no');
# See issue #2352
###############################################################################

cmp_tree(
    msg => 'Normal focus up - should work for all options',
    layout_before => 'S[a b*] V[c d T[e f g]]',
    layout_after => 'S[a* b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus up';
    });
cmp_tree(
    msg => 'Normal focus right - should work for all options',
    layout_before => 'S[a b] V[c d T[e* f g]]',
    layout_after => 'S[a b] V[c d T[e f* g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus right';
    });
cmp_tree(
    msg => 'Focus leaves workspace vertically',
    layout_before => 'S[a b*] V[c d T[e f g]]',
    layout_after => 'S[a b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus down';
        is(focused_ws, 'left-bottom', 'Correct workspace focused');
    });
cmp_tree(
    msg => 'Focus does not wrap vertically',
    layout_before => 'S[a* b] V[c d T[e f g]]',
    layout_after => 'S[a* b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus up';
    });
cmp_tree(
    msg => 'Focus does not wrap horizontally',
    layout_before => 'S[a b*] V[c d T[e f g]]',
    layout_after => 'S[a b*] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus left';
    });
cmp_tree(
    msg => 'Directional focus in the orientation of the parent does not wrap',
    layout_before => 'S[a b] V[c d T[e* f g]]',
    layout_after => 'S[a b*] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus left';
    });
cmp_tree(
    msg => 'Focus leaves workspace horizontally',
    layout_before => 'S[a b] V[c d* T[e f g]]',
    layout_after => 'S[a b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus right';
        is(focused_ws, 'right-top', 'Correct workspace focused');
    });

###############################################################################
focus_wrapping('force');
###############################################################################

cmp_tree(
    msg => 'Normal focus up - should work for all options',
    layout_before => 'S[a b*] V[c d T[e f g]]',
    layout_after => 'S[a* b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus up';
    });
cmp_tree(
    msg => 'Normal focus right - should work for all options',
    layout_before => 'S[a b] V[c d T[e* f g]]',
    layout_after => 'S[a b] V[c d T[e f* g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus right';
    });
cmp_tree(
    msg => 'Focus does not leave workspace vertically',
    layout_before => 'S[a b*] V[c d T[e f g]]',
    layout_after => 'S[a* b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus down';
        is(focused_ws, 'left-top', 'Correct workspace focused');
    });
cmp_tree(
    msg => 'Focus wraps vertically',
    layout_before => 'S[a* b] V[c d T[e f g]]',
    layout_after => 'S[a b*] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus up';
    });
cmp_tree(
    msg => 'Focus wraps horizontally (focus direction different than parent\'s orientation)',
    layout_before => 'S[a b*] V[c d T[e f g]]',
    layout_after => 'S[a b] V[c d T[e f g*]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus left';
    });
cmp_tree(
    msg => 'Directional focus in the orientation of the parent wraps',
    layout_before => 'S[a b] V[c d T[e* f g]]',
    layout_after => 'S[a b] V[c d T[e f g*]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus left';
    });
cmp_tree(    # 'focus_wrapping force' exclusive test
    msg => 'But leaves when selecting parent',
    layout_before => 'S[a b] V[c d T[e* f g]]',
    layout_after => 'S[a b*] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus parent, focus right';
    });
cmp_tree(
    msg => 'Focus does not leave workspace horizontally',
    layout_before => 'S[a b] V[c d* T[e f g]]',
    layout_after => 'S[a b*] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus right';
        is(focused_ws, 'left-top', 'Correct workspace focused');
    });
cmp_tree(    # 'focus_wrapping force|workspace' exclusive test
    msg => 'But leaves when selecting parent x2',
    layout_before => 'S[a b] V[c d* T[e f g]]',
    layout_after => 'S[a b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus parent, focus parent, focus right';
        is(focused_ws, 'right-top', 'Correct workspace focused');
    });

###############################################################################
focus_wrapping('workspace');
# See issue #2180
###############################################################################

cmp_tree(
    msg => 'Normal focus up - should work for all options',
    layout_before => 'S[a b*] V[c d T[e f g]]',
    layout_after => 'S[a* b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus up';
    });
cmp_tree(
    msg => 'Normal focus right - should work for all options',
    layout_before => 'S[a b] V[c d T[e* f g]]',
    layout_after => 'S[a b] V[c d T[e f* g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus right';
    });
cmp_tree(
    msg => 'Focus does not leave workspace vertically',
    layout_before => 'S[a b*] V[c d T[e f g]]',
    layout_after => 'S[a* b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus down';
        is(focused_ws, 'left-top', 'Correct workspace focused');
    });
cmp_tree(
    msg => 'Focus wraps vertically',
    layout_before => 'S[a* b] V[c d T[e f g]]',
    layout_after => 'S[a b*] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus up';
    });
cmp_tree(
    msg => 'Focus wraps horizontally',
    layout_before => 'S[a b*] V[c d T[e f g]]',
    layout_after => 'S[a b] V[c d T[e f g*]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus left';
    });
cmp_tree(
    msg => 'Directional focus in the orientation of the parent does not wrap',
    layout_before => 'S[a b] V[c d T[e* f g]]',
    layout_after => 'S[a b*] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus left';
    });
cmp_tree(
    msg => 'Focus does not leave workspace horizontally',
    layout_before => 'S[a b] V[c d* T[e f g]]',
    layout_after => 'S[a b*] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus right';
        is(focused_ws, 'left-top', 'Correct workspace focused');
    });
cmp_tree(    # 'focus_wrapping force|workspace' exclusive test
    msg => 'But leaves when selecting parent x2',
    layout_before => 'S[a b] V[c d* T[e f g]]',
    layout_after => 'S[a b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        cmd 'focus parent, focus parent, focus right';
        is(focused_ws, 'right-top', 'Correct workspace focused');
    });

cmp_tree(    # 'focus_wrapping workspace' exclusive test
    msg => 'x',
    layout_before => 'S[a* b] V[c d T[e f g]]',
    layout_after => 'S[a b] V[c d T[e f g]]',
    ws => 'left-top',
    cb => sub {
        subtest 'random tests' => sub {
            my @directions = qw(left right top down);
            for my $i (1 .. 50) {
                my $direction = $directions[rand @directions];
                cmd "focus $direction";

                return unless is(focused_ws, 'left-top', "'focus $direction' did not change workspace");
            }
        };
    });

exit_gracefully($pid);

done_testing;
