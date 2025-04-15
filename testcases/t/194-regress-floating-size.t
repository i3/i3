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
# Verifies that the size requested by floating windows is set by i3, no matter
# to which value the new_window option is set.
# ticket #770, bug still present in commit ae88accf6fe3817ff42d0d51be1965071194766e
use i3test i3_autostart => 0;

sub test_with_config {
    my ($value) = @_;

    my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

    if (defined($value)) {
        $config .= "$value\n";
        diag("testing with $value");
    } else {
        diag("testing without new_window");
    }

    my $pid = launch_with_config($config);

    my $tmp = fresh_workspace;

    my $window = open_floating_window({ rect => [ 0, 0, 400, 150 ] });

    my ($absolute, $top) = $window->rect;

    ok($window->mapped, 'Window is mapped');
    cmp_ok($absolute->{width}, '==', 400, 'requested width kept');
    cmp_ok($absolute->{height}, '==', 150, 'requested height kept');

    exit_gracefully($pid);
}

test_with_config(undef);
test_with_config('new_window 1pixel');
test_with_config('new_window normal');
test_with_config('new_window none');
test_with_config('hide_edge_borders both');

done_testing;
