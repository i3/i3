#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Verifies that entire outputs can be saved and restored properly by i3.
# Ticket: #1306
# Bug still in: 4.8-26-gf96ec19
use i3test;
use File::Temp qw(tempfile);
use IO::Handle;

my $ws = fresh_workspace;

################################################################################
# Append a new workspace with a name.
################################################################################

ok(!workspace_exists('ws_new'), 'workspace "ws_new" does not exist yet');

my ($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<'EOT';
// vim:ts=4:sw=4:et
{
    // workspace with 1 children
    "border": "pixel",
    "floating": "auto_off",
    "layout": "splith",
    "percent": null,
    "type": "workspace",
    "name": "ws_new",
    "nodes": [
        {
            "border": "pixel",
            "floating": "auto_off",
            "geometry": {
               "height": 268,
               "width": 484,
               "x": 0,
               "y": 0
            },
            "name": "vals@w00t: ~",
            "percent": 1,
            "swallows": [
               {
               // "class": "^URxvt$",
               // "instance": "^urxvt$",
               // "title": "^vals\\@w00t\\:\\ \\~$"
               }
            ],
            "type": "con"
        }
    ]
}
EOT
$fh->flush;
cmd "append_layout $filename";

ok(workspace_exists('ws_new'), 'workspace "ws_new" exists now');

does_i3_live;

close($fh);

################################################################################
# Append a new workspace with a name that clashes with an existing workspace.
################################################################################

my @old_workspaces = @{get_workspace_names()};

cmd "append_layout $filename";

my @new_workspaces = @{get_workspace_names()};
cmp_ok(scalar @new_workspaces, '>', scalar @old_workspaces, 'more workspaces than before');

my %created_workspaces = map { ($_, 1) } @new_workspaces;
delete $created_workspaces{$_} for @old_workspaces;
diag('created workspaces = ' . Dumper(keys %created_workspaces));
cmp_ok(scalar keys %created_workspaces, '>', 0, 'new workspaces appeared');

################################################################################
# Append a new workspace without a name.
################################################################################

ok(!workspace_exists('unnamed'), 'workspace "unnamed" does not exist yet');

($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<'EOT';
// vim:ts=4:sw=4:et
{
    // workspace with 1 children
    "border": "pixel",
    "floating": "auto_off",
    "layout": "splith",
    "percent": null,
    "type": "workspace",
    "nodes": [
        {
            "border": "pixel",
            "floating": "auto_off",
            "geometry": {
               "height": 268,
               "width": 484,
               "x": 0,
               "y": 0
            },
            "name": "vals@w00t: ~",
            "percent": 1,
            "swallows": [
               {
               // "class": "^URxvt$",
               // "instance": "^urxvt$",
               // "title": "^vals\\@w00t\\:\\ \\~$"
               }
            ],
            "type": "con"
        }
    ]
}
EOT
$fh->flush;
cmd "append_layout $filename";

ok(workspace_exists('unnamed'), 'workspace "unnamed" exists now');

################################################################################
# Append a workspace with a numeric name, ensure it has ->num set.
################################################################################

ok(!workspace_exists('4'), 'workspace "4" does not exist yet');

($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<'EOT';
// vim:ts=4:sw=4:et
{
    // workspace with 1 children
    "border": "pixel",
    "floating": "auto_off",
    "layout": "splith",
    "percent": null,
    "type": "workspace",
    "name": "4",
    "nodes": [
        {
            "border": "pixel",
            "floating": "auto_off",
            "geometry": {
               "height": 268,
               "width": 484,
               "x": 0,
               "y": 0
            },
            "name": "vals@w00t: ~",
            "percent": 1,
            "swallows": [
               {
               // "class": "^URxvt$",
               // "instance": "^urxvt$",
               // "title": "^vals\\@w00t\\:\\ \\~$"
               }
            ],
            "type": "con"
        }
    ]
}
EOT
$fh->flush;
cmd "append_layout $filename";

ok(workspace_exists('4'), 'workspace "4" exists now');
my $ws = get_ws("4");
is($ws->{num}, 4, 'workspace number is 4');

done_testing;
