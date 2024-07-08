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
# Tests that new workspace names are taken from the config,
# then from the first free number starting with 1.
#
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0

bindsym 1 workspace 1: eggs
EOT

my $i3 = i3(get_socket_path());
my $ws = $i3->get_workspaces->recv;

is($ws->[0]->{name}, '1: eggs', 'new workspace uses config name');
is($ws->[1]->{name}, '2', 'naming continues with next free number');

done_testing;
