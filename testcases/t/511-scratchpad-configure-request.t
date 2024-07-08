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
# Tests that ConfigureRequests don’t make windows fall out of the scratchpad.
# Ticket: #898
# Bug still in: 4.4-15-g770ead6
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

my $left_ws = fresh_workspace(output => 0);
my $right_ws = fresh_workspace(output => 1);

my $window = open_window;
cmd 'move scratchpad';

# Cause a ConfigureRequest by setting the window’s position/size.
my ($a, $t) = $window->rect;
$window->rect(X11::XCB::Rect->new(x => 0, y => 0, width => $a->width, height => $a->height));

sync_with_i3;

my $ws = get_ws($left_ws);
is(scalar @{$ws->{floating_nodes}}, 0, 'scratchpad window still in scratchpad after ConfigureRequest');
$ws = get_ws($right_ws);
is(scalar @{$ws->{floating_nodes}}, 0, 'scratchpad window still in scratchpad after ConfigureRequest');

done_testing;
