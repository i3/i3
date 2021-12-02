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
# Verifies title_window_icon behavior.
use i3test i3_autostart => 0;

sub window_icon_padding {
    my ($ws) = @_;
    my ($nodes, $focus) = get_ws_content($ws);
    ok(@{$nodes} == 1, 'precisely one container on workspace');
    return $nodes->[0]->{'window_icon_padding'};
}

my $config = <<"EOT";
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT
my $pid = launch_with_config($config);

my $tmp = fresh_workspace;

cmd 'open';
is(window_icon_padding($tmp), -1, 'window_icon_padding defaults to -1');

cmd 'title_window_icon on';
isnt(window_icon_padding($tmp), -1, 'window_icon_padding no longer -1');

cmd 'title_window_icon toggle';
is(window_icon_padding($tmp), -1, 'window_icon_padding back to -1');

cmd 'title_window_icon toggle';
isnt(window_icon_padding($tmp), -1, 'window_icon_padding no longer -1 again');

cmd 'title_window_icon off';
is(window_icon_padding($tmp), -1, 'window_icon_padding back to -1');

cmd 'title_window_icon padding 3px';
is(window_icon_padding($tmp), 3, 'window_icon_padding set to 3');

cmd 'title_window_icon toggle';
ok(window_icon_padding($tmp) < 0, 'window_icon_padding toggled off');

cmd 'title_window_icon toggle';
is(window_icon_padding($tmp), 3, 'window_icon_padding toggled back to 3');

cmd 'title_window_icon toggle 5px';
ok(window_icon_padding($tmp) < 0, 'window_icon_padding toggled off');

cmd 'title_window_icon toggle 5px';
is(window_icon_padding($tmp), 5, 'window_icon_padding toggled on to 5px');

cmd 'title_window_icon toggle 5px';
ok(window_icon_padding($tmp) < 0, 'window_icon_padding toggled off');

cmd 'title_window_icon toggle 4px';
is(window_icon_padding($tmp), 4, 'window_icon_padding toggled on to 4px');

exit_gracefully($pid);

################################################################################
# Verify title_window_icon can be used with for_window as expected
################################################################################

$config = <<"EOT";
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

for_window [class=".*"] title_window_icon padding 3px
EOT
$pid = launch_with_config($config);

$tmp = fresh_workspace;

open_window;
is(window_icon_padding($tmp), 3, 'window_icon_padding set to 3');

exit_gracefully($pid);

done_testing;
