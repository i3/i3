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
# Verifies that i3 does not crash when a layout is partially loadable.
# ticket #1145, bug still present in commit b109b1b20dd51401dc929407453d3acdd8ff5566
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
# file with a superfluous trailing comma
################################################################################

my $ws = fresh_workspace;

my @content = @{get_ws_content($ws)};
is(@content, 0, 'no nodes on the new workspace yet');

($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<'EOT';
// vim:ts=4:sw=4:et
{
    "border": "pixel",
    "floating": "auto_off",
    "geometry": {
       "height": 777,
       "width": 199,
       "x": 0,
       "y": 0
    },
    "name": "Buddy List",
    "percent": 0.116145833333333,
    "swallows": [
       {
       "class": "^Pidgin$",
       "window_role": "^buddy_list$"
       }
    ],
    "type": "con"
}

{
    // splitv split container with 1 children
    "border": "pixel",
    "floating": "auto_off",
    "layout": "splitv",
    "percent": 0.883854166666667,
    "swallows": [
       {}
    ],
    "type": "con",
    "nodes": [
        {
            // splitv split container with 2 children
            "border": "pixel",
            "floating": "auto_off",
            "layout": "splitv",
            "percent": 1,
            "swallows": [
               {}
            ],
            "type": "con",
            "nodes": [
                {
                    "border": "pixel",
                    "floating": "auto_off",
                    "geometry": {
                       "height": 318,
                       "width": 566,
                       "x": 0,
                       "y": 0
                    },
                    "name": "zsh",
                    "percent": 0.5,
                    "swallows": [
                       {
                         "class": "^URxvt$",
                         "instance": "^IRC$",
                       }
                    ],
                    "type": "con"
                },
                {
                    "border": "pixel",
                    "floating": "auto_off",
                    "geometry": {
                       "height": 1057,
                       "width": 636,
                       "x": 0,
                       "y": 0
                    },
                    "name": "Michael Stapelberg",
                    "percent": 0.5,
                    "swallows": [
                       {
                         "class": "^Pidgin$",
                         "window_role": "^conversation$"
                       }
                    ],
                    "type": "con"
                }
            ]
        }
    ]
}

EOT
$fh->flush;
my $reply = cmd "append_layout $filename";
ok(!$reply->[0]->{success}, 'IPC reply did not indicate success');

does_i3_live;


close($fh);

################################################################################
# another file with a superfluous trailing comma (issue #2755)
################################################################################

subtest 'issue 2755' => sub {
    plan tests => 4;
    $ws = fresh_workspace;

    @content = @{get_ws_content($ws)};
    is(@content, 0, 'no nodes on the new workspace yet');

    ($fh, $filename) = tempfile(UNLINK => 1);
    print $fh <<'EOT';
// vim:ts=4:sw=4:et
{
    // splith split container with 2 children
    "border": "normal",
    "floating": "auto_off",
    "layout": "splith",
    "percent": null,
    "type": "con",
    "nodes": [
        {
            "border": "normal",
            "current_border_width": 2,
            "floating": "auto_off",
            "geometry": {
               "height": 860,
               "width": 1396,
               "x": 1922,
               "y": 38
            },
            "name": "Chromium1",
            "percent": 0.5,
            "swallows": [
               {
                  "class": "^Chromium$",
               // "instance": "^chromium$",
               // "title": "^Git\\ Tutorial\\ \\-\\ corp\\ \\-\\ Chromium$",
               // "transient_for": "^$",
               // "window_role": "^browser$"
               }
            ],
            "type": "con"
        },
        {
            "border": "normal",
            "current_border_width": 2,
            "floating": "auto_off",
            "geometry": {
               "height": 1040,
               "width": 956,
               "x": 2,
               "y": 38
            },
            "name": "Chromium2",
            "percent": 0.5,
            "swallows": [
               {
                  "class": "^Chromium$",
               // "instance": "^chromium$",
               // "title": "^Nutanix\\ \\-\\ Prod\\ \\-\\ Sign\\ In\\ \\-\\ Chromium$",
               // "transient_for": "^$",
               // "window_role": "^browser$"
               }
            ],
            "type": "con"
        }
    ]
}

EOT
    $fh->flush;
    $reply = cmd "append_layout $filename";
    ok(!$reply->[0]->{success}, 'IPC reply did not indicate success');

    does_i3_live;

    # Move to a different workspace rendered the half-attached con’s con->parent
    # invalid.
    fresh_workspace;

    cmd '[urgent=latest] focus';
    $reply = cmd 'scratchpad show';

    does_i3_live;

    close($fh);
};

################################################################################
# wrong percent key in a child node
################################################################################

$ws = fresh_workspace;

@content = @{get_ws_content($ws)};
is(@content, 0, 'no nodes on the new workspace yet');

($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<'EOT';
// vim:ts=4:sw=4:et
{
    "border": "pixel",
    "floating": "auto_off",
    "layout": "splitv",
    "type": "con",
    "nodes": [
        {
            "border": "pixel",
            "floating": "auto_off",
            "geometry": {
               "height": 318,
               "width": 566,
               "x": 0,
               "y": 0
            },
            "name": "zsh",
            "percent": 0.833333,
            "swallows": [
               {
                 "class": "^URxvt$",
                 "instance": "^IRC$"
               }
            ],
            "type": "con"
        }
    ]
}

EOT
$fh->flush;
cmd "append_layout $filename";

does_i3_live;

close($fh);

################################################################################
# Issue with floating key being set, without proper parent
# See #3901
################################################################################
subtest 'issue 3901' => sub {
    kill_all_windows;
    $ws = fresh_workspace;
    is(scalar @{get_ws($ws)->{floating_nodes}}, 0, 'No floating nodes yet');

    ($fh, $filename) = tempfile(UNLINK => 1);
    print $fh <<'EOT';
// vim:ts=4:sw=4:et
{
    "border": "pixel",
    "current_border_width": 1,
    "floating": "auto_on", // crashes: user_on, auto_on, no crash: user_off, auto_off
    "geometry": {
       "height": 400,
       "width": 300,
       "x": 820,
       "y": 350
    },
    "name": "Click me to crash",
    "percent": 0.5, // still crashes if this field is absent
    "swallows": [
       {
       "class": "^this doesn't matter as long as it doesn't match a new window$"
       }
    ],
    "type": "con"
}

EOT
    $fh->flush;
    $reply = cmd "append_layout $filename";
    ok($reply->[0]->{success}, 'IPC reply indicated success');

    cmd '[floating] focus';
    is(scalar @{get_ws($ws)->{floating_nodes}}, 1, 'one floating node on this ws');

    does_i3_live;

    close($fh);
};

done_testing;
