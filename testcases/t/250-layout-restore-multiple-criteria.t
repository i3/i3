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
# TODO: Description of this file.
# Ticket: #1817
# Bug still in: 4.10.3-270-g0fb784f
use i3test;
use File::Temp qw(tempfile);
use IO::Handle;

my $ws = fresh_workspace;

my @content = @{get_ws_content($ws)};
is(@content, 0, 'no nodes on the new workspace yet');

my ($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<'EOT';
{
    // stacked split container with 1 children
    "border": "normal",
    "floating": "auto_off",
    "layout": "stacked",
    "percent": 0.40,
    "type": "con",
    "nodes": [
        {
            "border": "normal",
            "current_border_width": 2,
            "floating": "auto_off",
            "geometry": {
               "height": 460,
               "width": 804,
               "x": 0,
               "y": 0
            },
            // "name": "",
            "percent": 0.5,
            "swallows": [
               { "class": "^URxvt$" },
               { "class": "^Gitk$" },
               { "class": "^Git-gui$" }
            ],
            "type": "con"
        }
    ]
}
EOT
$fh->flush;
my $reply = cmd "append_layout $filename";
close($fh);

does_i3_live;

@content = @{get_ws_content($ws)};
is(@content, 1, "one node on the workspace now");

my $should_swallow = open_window(wm_class => 'URxvt');

@content = @{get_ws_content($ws)};
is(@content, 1, "still one node on the workspace now");
my @nodes = @{$content[0]->{nodes}};
is($nodes[0]->{window}, $should_swallow->id, "swallowed window on top");

cmd 'focus parent';

my $should_ignore = open_window(wm_class => 'Gitk');

@content = @{get_ws_content($ws)};
is(@content, 2, "two nodes on the workspace");

done_testing;
