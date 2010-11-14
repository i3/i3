#!perl
# vim:ts=4:sw=4:expandtab

use i3test tests => 3;
use Time::HiRes qw(sleep);

my $i3 = i3("/tmp/nestedcons");

my $tmp = get_unused_workspace();
$i3->command("workspace $tmp")->recv;

$i3->command("open")->recv;
$i3->command("open")->recv;

my @content = @{get_ws_content($tmp)};
cmp_ok(@content, '==', 2, 'two containers opened');
cmp_ok($content[1]->{focused}, '==', 1, 'Second container focused');

$i3->command("mode floating")->recv;
$i3->command("mode tiling")->recv;

@content = @{get_ws_content($tmp)};
cmp_ok($content[1]->{focused}, '==', 1, 'Second container still focused after mode toggle');
