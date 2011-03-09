#!perl
# vim:ts=4:sw=4:expandtab

use i3test;

my $tmp = fresh_workspace;

#############################################################################
# 1: see if focus stays the same when toggling tiling/floating mode
#############################################################################

cmd "open";
cmd "open";

my @content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 2, 'two containers opened');
cmp_ok($content[1]->{focused}, '==', 1, 'Second container focused');

cmd "mode floating";
cmd "mode tiling";

@content = @{get_ws_content($tmp)};
cmp_ok($content[1]->{focused}, '==', 1, 'Second container still focused after mode toggle');

#############################################################################
# 2: see if the focus gets reverted correctly when closing floating clients
# (first to the next floating client, then to the last focused tiling client)
#############################################################################

$tmp = fresh_workspace;

cmd "open";
cmd "open";
cmd "open";

@content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 3, 'two containers opened');
cmp_ok($content[2]->{focused}, '==', 1, 'Last container focused');

my $last_id = $content[2]->{id};
my $second_id = $content[1]->{id};
my $first_id = $content[0]->{id};
diag("last_id = $last_id, second_id = $second_id, first_id = $first_id");

cmd qq|[con_id="$second_id"] focus|;
@content = @{get_ws_content($tmp)};
cmp_ok($content[1]->{focused}, '==', 1, 'Second container focused');

cmd "mode floating";

cmd qq|[con_id="$last_id"] focus|;
@content = @{get_ws_content($tmp)};
cmp_ok($content[1]->{focused}, '==', 1, 'Last container focused');

cmd "mode floating";

diag("focused = " . get_focused($tmp));

cmd "kill";

diag("focused = " . get_focused($tmp));
# TODO: this test result is actually not right. the focus reverts to where the mouse pointer is.
cmp_ok(get_focused($tmp), '==', $second_id, 'Focus reverted to second floating container');

cmd "kill";
@content = @{get_ws_content($tmp)};
cmp_ok($content[0]->{focused}, '==', 1, 'Focus reverted to tiling container');

done_testing;
