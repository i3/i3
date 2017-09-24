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
# Tests all supported criteria for the "swallows" key.
use i3test;
use File::Temp qw(tempfile);
use IO::Handle;
use X11::XCB qw(PROP_MODE_REPLACE);

sub verify_swallow_criterion {
    my ($cfgline, $open_window_cb) = @_;

    my $ws = fresh_workspace;

    my @content = @{get_ws_content($ws)};
    is(@content, 0, "no nodes on the new workspace yet ($cfgline)");

    my ($fh, $filename) = tempfile(UNLINK => 1);
    print $fh <<EOT;
{
    "layout": "splitv",
    "nodes": [
        {
            "swallows": [
                {
                    $cfgline
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
    is(@content, 1, "one node on the workspace now ($cfgline)");

    my $top = $open_window_cb->();

    @content = @{get_ws_content($ws)};
    is(@content, 1, "still one node on the workspace now ($cfgline)");
    my @nodes = @{$content[0]->{nodes}};
    is($nodes[0]->{window}, $top->id, "top window on top ($cfgline)");

    close($fh);
}

verify_swallow_criterion(
    '"class": "^special_class$"',
    sub { open_window(wm_class => 'special_class') }
);

# Run the same test again to verify that the window is not being swallowed by
# the first container. Each swallow condition should only swallow precisely one
# window.
verify_swallow_criterion(
    '"class": "^special_class$"',
    sub { open_window(wm_class => 'special_class') }
);

verify_swallow_criterion(
    '"instance": "^special_instance$"',
    sub { open_window(wm_class => '', instance => 'special_instance') }
);

verify_swallow_criterion(
    '"title": "^special_title$"',
    sub { open_window(name => 'special_title') }
);

verify_swallow_criterion(
    '"window_role": "^special_role$"',
    sub {
        open_window(
            name => 'roletest',
            before_map => sub {
                my ($window) = @_;
                my $atomname = $x->atom(name => 'WM_WINDOW_ROLE');
                my $atomtype = $x->atom(name => 'STRING');
                $x->change_property(
                    PROP_MODE_REPLACE,
                    $window->id,
                    $atomname->id,
                    $atomtype->id,
                    8,
                    length("special_role") + 1,
                    "special_role\x00"
                );
            },
        );
    }
);

done_testing;
