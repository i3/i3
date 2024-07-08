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
# Tests for using X resources in the config.
# Ticket: #2130
use i3test i3_autostart => 0;
use X11::XCB qw(PROP_MODE_REPLACE);

sub get_marks {
    return i3(get_socket_path())->get_marks->recv;
}

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

# This isn't necessarily what X resources are intended for, but it'll do the
# job for the test.
set_from_resource \$mark i3wm.mark none
for_window [class=worksforme] mark \$mark

set_from_resource \$othermark i3wm.doesnotexist none
for_window [class=doesnotworkforme] mark \$othermark

EOT

$x->change_property(
    PROP_MODE_REPLACE,
    $x->get_root_window(),
    $x->atom(name => 'RESOURCE_MANAGER')->id,
    $x->atom(name => 'STRING')->id,
    32,
    length('*mark: works'),
    '*mark: works');
$x->flush;

my $pid = launch_with_config($config);

open_window(wm_class => 'worksforme');
sync_with_i3;
is_deeply(get_marks(), [ 'works' ], 'the resource has loaded correctly');

cmd 'kill';

open_window(wm_class => 'doesnotworkforme');
sync_with_i3;
is_deeply(get_marks(), [ 'none' ], 'the resource fallback was used');

exit_gracefully($pid);

done_testing;
