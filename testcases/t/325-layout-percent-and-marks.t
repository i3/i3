#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests that container percentages are correctly preserved when restoring
# layouts containing marks.
# See: https://github.com/i3/i3/issues/6391
#
use i3test;
use File::Temp qw(tempfile);
use IO::Handle;

################################################################################
# Test 1: Layout with marks should preserve percent values
################################################################################

my $ws = fresh_workspace;

my @content = @{get_ws_content($ws)};
is(@content, 0, 'no nodes on the new workspace yet');

my ($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<'EOT';
{
    "layout": "splith",
    "nodes": [
        {
            "percent": 0.2,
            "marks": ["left_mark"],
            "swallows": [
                { "class": "^left$" }
            ]
        },
        {
            "percent": 0.8,
            "swallows": [
                { "class": "^right$" }
            ]
        }
    ]
}
EOT
$fh->flush;
cmd "append_layout $filename";
close($fh);

does_i3_live;

@content = @{get_ws_content($ws)};
is(@content, 1, 'one node on the workspace now');

my @nodes = @{$content[0]->{nodes}};
is(@nodes, 2, 'split container has two children');

cmp_float($nodes[0]->{percent}, 0.2, 'first container (with mark) got 20%');
cmp_float($nodes[1]->{percent}, 0.8, 'second container got 80%');

my @marks = @{$nodes[0]->{marks}};
is(@marks, 1, 'first container has one mark');
is($marks[0], 'left_mark', 'mark is correctly applied');

done_testing;
