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
# Restores a simple layout from a JSON file.
use i3test;
use File::Temp qw(tempfile);
use IO::Handle;

################################################################################
# empty layout file.
################################################################################

my ($fh, $filename) = tempfile(UNLINK => 1);
cmd "append_layout $filename";

does_i3_live;

close($fh);

################################################################################
# simple vsplit layout
################################################################################

my $ws = fresh_workspace;

my @content = @{get_ws_content($ws)};
is(@content, 0, 'no nodes on the new workspace yet');

($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<EOT;
{
    "layout": "splitv",
    "nodes": [
        {
        },
        {
        }
    ]
}
EOT
$fh->flush;
cmd "append_layout $filename";

does_i3_live;

@content = @{get_ws_content($ws)};
is(@content, 1, 'one node on the workspace now');
is($content[0]->{layout}, 'splitv', 'node has splitv layout');
is(@{$content[0]->{nodes}}, 2, 'node has two children');

close($fh);

################################################################################
# two simple vsplit containers in the same file
################################################################################

$ws = fresh_workspace;

@content = @{get_ws_content($ws)};
is(@content, 0, 'no nodes on the new workspace yet');

($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<EOT;
{
    "layout": "splitv",
    "nodes": [
        {
        },
        {
        }
    ]
}

{
    "layout": "splitv"
}
EOT
$fh->flush;
cmd "append_layout $filename";

does_i3_live;

@content = @{get_ws_content($ws)};
is(@content, 2, 'one node on the workspace now');
is($content[0]->{layout}, 'splitv', 'first node has splitv layout');
is(@{$content[0]->{nodes}}, 2, 'first node has two children');
is($content[1]->{layout}, 'splitv', 'second node has splitv layout');
is(@{$content[1]->{nodes}}, 0, 'first node has no children');

close($fh);

################################################################################
# simple vsplit layout with swallow specifications
################################################################################

$ws = fresh_workspace;

@content = @{get_ws_content($ws)};
is(@content, 0, 'no nodes on the new workspace yet');

($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<EOT;
{
    "layout": "splitv",
    "nodes": [
        {
            "swallows": [
                {
                    "class": "top"
                }
            ]
        },
        {
            "swallows": [
                {
                    "class": "bottom"
                }
            ]
        }
    ]
}
EOT
$fh->flush;
cmd "append_layout $filename";

does_i3_live;

@content = @{get_ws_content($ws)};
is(@content, 1, 'one node on the workspace now');

my $top = open_window(
    name => 'top window',
    wm_class => 'top',
    instance => 'top',
);

my $bottom = open_window(
    name => 'bottom window',
    wm_class => 'bottom',
    instance => 'bottom',
);

@content = @{get_ws_content($ws)};
is(@content, 1, 'still one node on the workspace now');
my @nodes = @{$content[0]->{nodes}};
is($nodes[0]->{name}, 'top window', 'top window on top');

close($fh);

done_testing;
