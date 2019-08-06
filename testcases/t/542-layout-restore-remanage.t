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
# Tests that swallowing still works after a window gets managed and its property
# updated.
use i3test;
use File::Temp qw(tempfile);
use IO::Handle;
use X11::XCB qw(PROP_MODE_REPLACE);

sub change_window_title {
    my ($window, $title, $length) = @_;
    my $atomname = $x->atom(name => '_NET_WM_NAME');
    my $atomtype = $x->atom(name => 'UTF8_STRING');
    $length ||= length($title) + 1;
    $x->change_property(
        PROP_MODE_REPLACE,
        $window->id,
        $atomname->id,
        $atomtype->id,
        8,
        $length,
        $title
    );
    sync_with_i3;
}

my $ws = fresh_workspace;

my @content = @{get_ws_content($ws)};
is(@content, 0, 'no nodes on the new workspace yet');

my ($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<EOT;
{
    "layout": "splitv",
    "nodes": [
        {
            "swallows": [
                {
                    "title": "different_title"
                }
            ]
        }
    ]
}
EOT
$fh->flush;
cmd "append_layout $filename";

@content = @{get_ws_content($ws)};
is(@content, 1, 'one node on the workspace now');

my $window = open_window(
    name => 'original_title',
    wm_class => 'a',
);

@content = @{get_ws_content($ws)};
is(@content, 2, 'two nodes on the workspace now');

change_window_title($window, "different_title");

does_i3_live;

@content = @{get_ws_content($ws)};
my @nodes = @{$content[0]->{nodes}};
is(@content, 1, 'only one node on the workspace now');
is($nodes[0]->{name}, 'different_title', 'test window got swallowed');

close($fh);

done_testing;
