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
# Verifies the GET_BINDING_MODE IPC command
# Ticket: #3892
# Bug still in: 4.18-318-g50160eb1
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

mode "extra" {
    bindsym Mod1+x nop foo
}
EOT

my $i3 = i3(get_socket_path());
$i3->connect->recv;
# TODO: use the symbolic name for the command/reply type instead of the
# numerical 12:
my $binding_state = $i3->message(12, "")->recv;
is($binding_state->{name}, 'default', 'at startup, binding mode is default');

cmd 'mode extra';

$binding_state = $i3->message(12, "")->recv;
is($binding_state->{name}, 'extra', 'after switching, binding mode is extra');

done_testing;
