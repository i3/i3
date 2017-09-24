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
# Verifies that i3 removes swallows specifications for split containers.
# ticket #1149, bug still present in commit 2fea5ef82bd3528ed62681f9ac64f45830f4acdf
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
    "type": "con",
    "nodes": [
        {
            // splitv split container with 2 children
            "border": "pixel",
            "floating": "auto_off",
            "layout": "splitv",
            "percent": 1,
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
                         "instance": "^IRC$"
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

does_i3_live;

ok($reply->[0]->{success}, 'IPC reply indicates success');

my @nodes = @{get_ws_content($ws)};

is_deeply($nodes[0]->{swallows},
    [
    {
        class => '^Pidgin$',
        window_role => '^buddy_list$',
    },
    ],
    'swallows specification not parsed correctly');

is_deeply($nodes[1]->{swallows},
    [],
    'swallows specification empty on split container');

my @children = @{$nodes[1]->{nodes}->[0]->{nodes}};

is_deeply($children[0]->{swallows},
    [
    {
        class => '^URxvt$',
        instance => '^IRC$',
    },
    ],
    'swallows specification not parsed correctly');

is_deeply($children[1]->{swallows},
    [
    {
        class => '^Pidgin$',
        window_role => '^conversation$',
    },
    ],
    'swallows specification not parsed correctly');

close($fh);
done_testing;
