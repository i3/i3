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
# Test popup_during_fullscreen option
use i3test i3_autostart => 0;
use i3test::XTEST;

sub setup {
    kill_all_windows;

    my $ws = fresh_workspace;
    my $w1 = open_window;
    cmd 'fullscreen';
    is_num_fullscreen($ws, 1, 'sanity check: one fullscreen window');
    return ($ws, $w1);
}

sub open_transient_for {
    my $for = shift;
    my $w = open_window({ dont_map => 1, rect => [ 30, 30, 50, 50 ] });
    $w->transient_for($for);
    $w->map;
    sync_with_i3;
    return $w;
}

sub open_without_map_wait {
    my $w = open_window({ dont_map => 1 });
    $w->map;
    sync_with_i3;
    return $w;
}

################################################################################
# Test popup_during_fullscreen ignore
################################################################################

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

popup_during_fullscreen ignore
EOT
my $pid = launch_with_config($config);

my ($ws, $w1) = setup;

my $w2 = open_transient_for($w1);
is_num_fullscreen($ws, 1, 'still one fullscren window');
is($x->input_focus, $w1->id, 'fullscreen window still focused');

open_without_map_wait;
is_num_fullscreen($ws, 1, 'still one fullscren window');
is($x->input_focus, $w1->id, 'fullscreen window focused');

exit_gracefully($pid);

################################################################################
# Test popup_during_fullscreen leave_fullscreen
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

popup_during_fullscreen leave_fullscreen
EOT
$pid = launch_with_config($config);

($ws, $w1) = setup;

# Fullscreen disabled when transient windows open
$w2 = open_transient_for($w1);
is_num_fullscreen($ws, 0, 'no fullscren window');
# XXX: Arguably a bug but leave_fullscreen does not change focus
is($x->input_focus, $w1->id, 'fullscreen window focused');

# Fullscreen stays when regular windows open
$w1->fullscreen(1);
open_without_map_wait;
is_num_fullscreen($ws, 1, 'still one fullscreen window');
is($x->input_focus, $w1->id, 'fullscreen window focused');


exit_gracefully($pid);

################################################################################
# Test popup_during_fullscreen smart
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

popup_during_fullscreen smart
EOT
$pid = launch_with_config($config);

($ws, $w1) = setup;

# Fullscreen stays when transient windows open
$w2 = open_transient_for($w1);
is_num_fullscreen($ws, 1, 'still one fullscreen window');
is($x->input_focus, $w2->id, 'popup focused');

# Fullscreen stays when regular windows open
open_without_map_wait;
is_num_fullscreen($ws, 1, 'still one fullscreen window');
is($x->input_focus, $w2->id, 'popup still focused');


exit_gracefully($pid);

################################################################################
# Test popup_during_fullscreen all
# See #6062
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

popup_during_fullscreen all
EOT
$pid = launch_with_config($config);

($ws, $w1) = setup;

# Fullscreen stays when transient windows open
$w2 = open_transient_for($w1);
is_num_fullscreen($ws, 1, 'still one fullscreen window');
is($x->input_focus, $w2->id, 'popup focused');

# Fullscreen stays when regular windows open
$w1->fullscreen(1);
open_without_map_wait;
is_num_fullscreen($ws, 1, 'still one fullscreen window');

exit_gracefully($pid);

done_testing;
