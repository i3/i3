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
# Test that moving a container that is to be flattened does not crash i3
# Ticket: #3831
# Bug still in: 4.17-199-ga638e0408
use i3test;

cmp_tree(
    msg => 'Moving a redundant container that is to be flattened does not crash i3',
    layout_before => 'a H[V[b* c]]',    # Not very attached to this result but
    layout_after => 'H[a] b* c',        # mainly checking if the crash happens.
    cb => sub {
        cmd 'focus parent, focus parent, move down';
        does_i3_live;
        is(get_ws(focused_ws)->{layout}, 'splitv', 'Workspace changed to splitv');
    });

cmp_tree(
    msg => "Same but create the redundant container with a 'split h' command",
    layout_before => 'a V[b* c]',
    layout_after => 'H[a] b* c',
    cb => sub {
        cmd 'focus parent, split h, focus parent, move down';
        does_i3_live;
        is(get_ws(focused_ws)->{layout}, 'splitv', 'Workspace changed to splitv');
    });


done_testing;
