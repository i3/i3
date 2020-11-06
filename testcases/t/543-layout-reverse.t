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
# Tests reverse layouts.
use i3test;
use File::Temp qw(tempfile);
use IO::Handle;

my $tmp = fresh_workspace;

################################################################################
# Make sure cmp_tree() knows what to do with reverse layouts
################################################################################

cmp_tree(
    msg => "Reverse layout sanity check for cmp_tree()",
    ws => $tmp,
    layout_before => 'V[c d* Tr[e f g]]',
    layout_after => 'V[c d Tr[e f g]]',
    cb => sub {
        cmd 'focus next sibling';
    });

my @content = @{get_ws_content($tmp)};
is($content[0]->{nodes}[2]->{layout_fill_order}, 'reverse', 'manually check that layout created by cmp_tree has to correct fill order');

################################################################################
# Make sure fill_order is correctly restored.
################################################################################

ok(!workspace_exists('ws_reverse'), 'workspace "ws_reverse" does not exist yet');

my ($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<'EOT';
// vim:ts=4:sw=4:et
{
    "border": "pixel",
    "floating": "auto_off",
    "layout": "splith",
    "type": "workspace",
    "name": "ws_reverse",
    "layout_fill_order": "reverse",
    "nodes": [
        {
            "border": "pixel",
            "floating": "auto_off",
            "layout": "tabbed",
            "type": "con",
            "layout_fill_order": "default",
            "nodes": [
                {
                    "border": "pixel",
                    "floating": "auto_off",
                    "layout": "splith",
                    "type": "con",
                    "layout_fill_order": "default",
                    "nodes": [
                        {
                            "border": "pixel",
                            "floating": "auto_off",
                            "type": "con"
                        },
                        {
                            "border": "pixel",
                            "floating": "auto_off",
                            "type": "con"
                        }
                    ]
                },
                {
                    "border": "pixel",
                    "floating": "auto_off",
                    "type": "con"
                }
            ]
        },
        {
            "border": "pixel",
            "floating": "auto_off",
            "layout": "splitv",
            "type": "con",
            "layout_fill_order": "reverse"
        }
    ]
}
EOT
$fh->flush;
cmd "append_layout $filename";

ok(workspace_exists('ws_reverse'), 'workspace "ws_reverse" exists now');

my $ws = get_ws('ws_reverse');
is($ws->{layout_fill_order}, 'reverse', 'workspace fill order is reverse');

@content = @{$ws->{nodes}};
is(@content, 2, 'workspace has two nodes');
is($content[0]->{layout_fill_order}, 'default', 'child node one has default fill order');
is($content[1]->{layout_fill_order}, 'reverse', 'child node two has reverse fill order');

################################################################################
# Test that con_get_tree_representation returns the correct layout string
################################################################################

# TODO Unable to find a method to the i3-frame titles

################################################################################
# Test that the layout fill_order command works on workspaces
################################################################################

$tmp = fresh_workspace;

$ws = get_ws($tmp);
is($ws->{layout_fill_order}, 'default', 'ensure the default layout fill order');

cmd 'layout fill_order reverse';
$ws = get_ws($tmp);
is($ws->{layout_fill_order}, 'reverse', 'cmd "layout fill_order reverse" changes layout_fill_order to "reverse"');

cmd 'layout fill_order default';
$ws = get_ws($tmp);
is($ws->{layout_fill_order}, 'default', 'cmd "layout fill_order default" changes layout_fill_order to "default"');

cmd 'layout fill_order toggle';
$ws = get_ws($tmp);
is ($ws->{layout_fill_order}, 'reverse', 'cmd "layout fill_order toggle" changes layout_fill_order from "default" -> "reverse"');
cmd 'layout fill_order toggle';
$ws = get_ws($tmp);
is ($ws->{layout_fill_order}, 'default', 'cmd "layout fill_order toggle" changes layout_fill_order from "reverse" -> "default"');

cmd 'layout fill_order foobar';
$ws = get_ws($tmp);
is ($ws->{layout_fill_order}, 'default', 'nonsensical layout fill_order option leaves fill order at default');

################################################################################
# Test the layout fill_order command works on containers down the tree
################################################################################

$tmp = fresh_workspace;

open_window;
open_window;
cmd 'split v';
open_window;

my ($nodes, $focus) = get_ws_content($tmp);
is($nodes->[1]->{layout_fill_order}, 'default', 'layout fill order is default currently');

cmd 'layout fill_order reverse';
($nodes, $focus) = get_ws_content($tmp);
is($nodes->[1]->{layout_fill_order}, 'reverse', 'layout fill order is reverse now');

cmd 'layout fill_order toggle';
($nodes, $focus) = get_ws_content($tmp);
is($nodes->[1]->{layout_fill_order}, 'default', 'layout fill order was toggled to default');

done_testing;
