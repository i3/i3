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
# Tests for _NET_WM_DESKTOP.
# Ticket: #2153
use i3test i3_autostart => 0;
use X11::XCB qw(:all);

sub get_net_wm_desktop {
    sync_with_i3;

    my ($con) = @_;
    my $cookie = $x->get_property(
        0,
        $con->{id},
        $x->atom(name => '_NET_WM_DESKTOP')->id,
        $x->atom(name => 'CARDINAL')->id,
        0,
        1
    );

    my $reply = $x->get_property_reply($cookie->{sequence});
    return undef if $reply->{length} != 1;

    return unpack("L", $reply->{value});
}

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

workspace "0" output "fake-0"
workspace "1" output "fake-0"
workspace "2" output "fake-0"
workspace "10" output "fake-1"
workspace "11" output "fake-1"
workspace "12" output "fake-1"

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

###############################################################################
# _NET_WM_DESKTOP is updated when the window is moved to another workspace
# on another output.
###############################################################################

my $pid = launch_with_config($config);

cmd 'workspace 0';
open_window;
cmd 'workspace 10';
open_window;
cmd 'workspace 0';
my $con = open_window;

cmd 'move window to workspace 10';

is(get_net_wm_desktop($con), 1, '_NET_WM_DESKTOP is updated when moving the window');

exit_gracefully($pid);

done_testing;
