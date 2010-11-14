#!perl
# vim:ts=4:sw=4:expandtab

use i3test tests => 9;
use Time::HiRes qw(sleep);

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

#############################################################################
# 1: see if focus stays the same when toggling tiling/floating mode
#############################################################################

$i3->command("open")->recv;
$i3->command("open")->recv;

my @content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 2, 'two containers opened');
cmp_ok($content[1]->{focused}, '==', 1, 'Second container focused');

$i3->command("mode floating")->recv;
$i3->command("mode tiling")->recv;

@content = @{get_ws_content($tmp)};
cmp_ok($content[1]->{focused}, '==', 1, 'Second container still focused after mode toggle');

#############################################################################
# 2: see if the focus gets reverted correctly when closing floating clients
# (first to the next floating client, then to the last focused tiling client)
#############################################################################

$tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

$i3->command("open")->recv;
$i3->command("open")->recv;
$i3->command("open")->recv;

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 3, 'two containers opened');
cmp_ok($content[2]->{focused}, '==', 1, 'Last container focused');

my $last_id = $content[2]->{id};
my $second_id = $content[1]->{id};
my $first_id = $content[0]->{id};
diag("last_id = $last_id, second_id = $second_id, first_id = $first_id");

$i3->command(qq|[con_id="$second_id"] focus|)->recv;
@content = @{get_ws_content($tmp)};
cmp_ok($content[1]->{focused}, '==', 1, 'Second container focused');

$i3->command("mode floating")->recv;

$i3->command(qq|[con_id="$last_id"] focus|)->recv;
@content = @{get_ws_content($tmp)};
cmp_ok($content[1]->{focused}, '==', 1, 'Last container focused');

$i3->command("mode floating")->recv;

diag("focused = " . get_focused($tmp));

$i3->command("kill")->recv;

diag("focused = " . get_focused($tmp));
# TODO: this test result is actually not right. the focus reverts to where the mouse pointer is.
cmp_ok(get_focused($tmp), '==', $second_id, 'Focus reverted to second floating container');

$i3->command("kill")->recv;
@content = @{get_ws_content($tmp)};
cmp_ok($content[0]->{focused}, '==', 1, 'Focus reverted to tiling container');
