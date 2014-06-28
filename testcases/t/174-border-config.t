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
# Tests the new_window and new_float config option.
#

use i3test i3_autostart => 0;

#####################################################################
# 1: check that new windows start with 'normal' border unless configured
# otherwise
#####################################################################

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

my $pid = launch_with_config($config);

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $first = open_window;

my @content = @{get_ws_content($tmp)};
ok(@content == 1, 'one container opened');
is($content[0]->{border}, 'normal', 'border normal by default');

exit_gracefully($pid);

#####################################################################
# 2: check that new tiling windows start with '1pixel' border when
# configured
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_window 1pixel
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$first = open_window;

@content = @{get_ws_content($tmp)};
ok(@content == 1, 'one container opened');
is($content[0]->{border}, 'pixel', 'border pixel by default');
is($content[0]->{current_border_width}, 1, 'border width pixels 1 (default)');

exit_gracefully($pid);

#####################################################################
# 3: check that new floating windows start with 'normal' border unless
# configured otherwise
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$first = open_floating_window;

my $wscontent = get_ws($tmp);
my @floating = @{$wscontent->{floating_nodes}};
ok(@floating == 1, 'one floating container opened');
my $floatingcon = $floating[0];
is($floatingcon->{nodes}->[0]->{border}, 'normal', 'border normal by default');

exit_gracefully($pid);

#####################################################################
# 4: check that new floating windows start with '1pixel' border when
# configured
#####################################################################

$config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

new_float 1pixel
EOT

$pid = launch_with_config($config);

$tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

$first = open_floating_window;

$wscontent = get_ws($tmp);
@floating = @{$wscontent->{floating_nodes}};
ok(@floating == 1, 'one floating container opened');
$floatingcon = $floating[0];
is($floatingcon->{nodes}->[0]->{border}, 'pixel', 'border pixel by default');

exit_gracefully($pid);

done_testing;
