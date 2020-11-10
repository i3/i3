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

############################################################
# Make sure window only gets swallowed once
############################################################
# Regression, issue #3888
$ws = fresh_workspace;

($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<'EOT';
// vim:ts=4:sw=4:et
{
    // splith split container with 2 children
    "layout": "splith",
    "type": "con",
    "nodes": [
        {
            "type": "con",
            "swallows": [
               {
               "class": "^foo$"
               }
            ]
        },
        {
            // splitv split container with 2 children
            "layout": "splitv",
            "type": "con",
            "nodes": [
                {
                    "type": "con",
                    "swallows": [
                       {
                        "class": "^foo$"
                       }
                    ]
                },
                {
                    "type": "con",
                    "swallows": [
                       {
                        "class": "^foo$"
                       }
                    ]
                }
            ]
        }
    ]
}
EOT
$fh->flush;
cmd "append_layout $filename";

$window = open_window wm_class => 'foo';

# Changing an unrelated window property originally resulted in the window
# getting remanaged and swallowd by a different placeholder, even though the
# matching property (class for the layout above) didn't change.
change_window_title($window, "different_title");

@content = @{get_ws_content($ws)};

subtest 'regression test that window gets only swallowed once', sub {
    is($content[0]->{nodes}[0]->{window}, $window->id, 'first placeholder swallowed window');
    isnt($content[0]->{nodes}[1]->{nodes}[0]->{window}, $window->id, 'second placeholder did not swallow window');
    isnt($content[0]->{nodes}[1]->{nodes}[1]->{window}, $window->id, 'thid placeholder did not swallow window');
};

done_testing;
