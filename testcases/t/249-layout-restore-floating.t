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
# Verifies floating containers are restored in the correct place in the
# hierarchy and get a non-zero size.
# Ticket: #1739
# Bug still in: 4.10.3-264-g419b73b
use i3test;
use File::Temp qw(tempfile);
use IO::Handle;

my $ws = fresh_workspace;

my @content = @{get_ws_content($ws)};
is(@content, 0, 'no nodes on the new workspace yet');

my ($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<'EOT';
// vim:ts=4:sw=4:et
{
    // floating_con with 1 children
    "border": "none",
    "floating": "auto_off",
    "layout": "splith",
    "percent": null,
    "type": "floating_con",
    // NOTE that "rect" is missing here.
    "nodes": [
        {
            "border": "none",
            "current_border_width": 0,
            "floating": "user_on",
            "geometry": {
               "height": 384,
               "width": 128,
               "x": 448,
               "y": 441
            },
            "name": "Splash",
            "percent": 1,
            "swallows": [
               {
                "class": "^Steam$",
                "instance": "^Steam$",
                "title": "^Steam$"
               // "transient_for": "^$"
               }
            ],
            "type": "con"
        }
    ]
}
EOT
$fh->flush;
my $reply = cmd "append_layout $filename";

does_i3_live;

ok($reply->[0]->{success}, 'Layout successfully loaded');

cmd 'kill';

ok(workspace_exists($ws), 'Workspace still exists');

does_i3_live;

close($fh);

done_testing;
