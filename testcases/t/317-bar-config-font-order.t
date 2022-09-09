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
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Verifies that bar config blocks get the i3-wide font configured,
# regardless of where the font is configured in the config file
# (before or after the bar config blocks).
# Ticket: #5031
# Bug still in: 4.20-105-g4db383e4
use i3test i3_config => <<'EOT';
# i3 config file (v4)

bar {
  # no font directive here, no i3-wide font configured (yet)
}

font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $i3 = i3(get_socket_path(0));
my $bars = $i3->get_bar_config()->recv;

my $bar_id = shift @$bars;
my $bar_config = $i3->get_bar_config($bar_id)->recv;
is($bar_config->{font}, 'fixed', 'font ok');

done_testing;
