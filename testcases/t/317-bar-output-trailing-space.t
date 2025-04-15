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
# Verifies that any trailing whitespace in strings (including in
# “bar { output <output> }” in particular) is stripped.
# Ticket: #5064
# Bug still in: 4.20-105-g4db383e4
use i3test i3_autostart => 0;

# Test with a single output.

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bar {
  output anything 
}
EOT

my $pid = launch_with_config($config);

my $i3 = i3(get_socket_path(0));
my $bars = $i3->get_bar_config()->recv;
is(@$bars, 1, 'one bar configured');
my $bar_id = shift @$bars;

my $bar_config = $i3->get_bar_config($bar_id)->recv;
is_deeply($bar_config->{outputs}, [ 'anything' ], 'outputs do not have trailing whitespace');

exit_gracefully($pid);

# Test with multiple outputs for a single bar.

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bar {
  output nospace
  output singlespace 
}
EOT

$pid = launch_with_config($config);

$i3 = i3(get_socket_path(0));
$bars = $i3->get_bar_config()->recv;
is(@$bars, 1, 'one bar configured');
$bar_id = shift @$bars;

$bar_config = $i3->get_bar_config($bar_id)->recv;
is_deeply($bar_config->{outputs}, [ 'nospace', 'singlespace' ], 'outputs do not have trailing whitespace');

exit_gracefully($pid);


done_testing;
