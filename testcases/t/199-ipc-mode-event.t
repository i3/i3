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
# Verifies that the IPC 'mode' event is sent when modes are changed.
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

mode "m1" {
    bindsym Mod1+x nop foo
}

mode "with spaces" {
    bindsym Mod1+y nop bar
}
EOT

my @events = events_for(
    sub { cmd 'mode "m1"' },
    'mode');

my @changes = map { $_->{change} } @events;
is_deeply(\@changes, [ 'm1' ], 'Mode event received');

done_testing;
