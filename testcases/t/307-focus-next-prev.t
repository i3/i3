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
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Test focus next|prev
# Ticket: #2587
use i3test;

cmp_tree(
    msg => "cmd 'prev' selects leaf 1/2",
    layout_before => 'S[a b] V[c d* T[e f g]]',
    layout_after => 'S[a b] V[c* d T[e f g]]',
    cb => sub {
        cmd 'focus prev';
    });

cmp_tree(
    msg => "cmd 'prev' selects leaf 2/2",
    layout_before => 'S[a b] V[c* d T[e f g]]',
    layout_after => 'S[a b*] V[c d T[e f g]]',
    cb => sub {
        # c* -> V -> b*
        cmd 'focus parent, focus prev';
    });

cmp_tree(
    msg => "cmd 'prev sibling' selects leaf again",
    layout_before => 'S[a b] V[c d* T[e f g]]',
    layout_after => 'S[a b] V[c* d T[e f g]]',
    cb => sub {
        cmd 'focus prev sibling';
    });

cmp_tree(
    msg => "cmd 'next' selects leaf",
    # Notice that g is the last to open before focus moves to d*
    layout_before => 'S[a b] V[c d* T[e f g]]',
    layout_after => 'S[a b] V[c d T[e f g*]]',
    cb => sub {
        cmd 'focus next';
    });

cmp_tree(
    msg => "cmd 'next sibling' selects parent 1/2",
    layout_before => 'S[a b] V[c d* T[e f g]]',
    layout_after => 'S[a b] V[c d T*[e f g]]',
    cb => sub {
        cmd 'focus next sibling';
    });

cmp_tree(
    msg => "cmd 'next sibling' selects parent 2/2",
    layout_before => 'S[a b*] V[c d T[e f g]]',
    layout_after => 'S[a b] V*[c d T[e f g]]',
    cb => sub {
        # b* -> S* -> V*
        cmd 'focus parent, focus next sibling';
    });

# See #3997
cmd 'workspace 2';
open_window;
cmd 'workspace 1';
open_window;
cmd 'focus parent, focus parent, focus next sibling, focus prev sibling';
does_i3_live;
is(focused_ws, '1', 'Back and forth between workspaces');

cmd 'focus parent, focus parent, focus next sibling';
is(focused_ws, '2', "Workspace 2 focused with 'focus next sibling'");

done_testing;
