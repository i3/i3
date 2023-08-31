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
# Make sure the trick used to move the container to its parent works.
# https://github.com/i3/i3/issues/1326#issuecomment-349082811
use i3test;

cmp_tree(
    msg => 'Move to parent when the parent is a workspace',
    layout_before => 'a H[b*] c',
    layout_after => 'a b* c',
    cb => sub {
        cmd 'mark _a, focus parent, focus parent, mark _b, [con_mark=_a] move window to mark _b, [con_mark=_a] focus';
    });

cmp_tree(
    msg => 'Move to parent when the parent is a split',
    layout_before => 'V[a H[b*] c]',
    layout_after => 'V[a b* c]',
    cb => sub {
        cmd 'mark _a, focus parent, focus parent, mark _b, [con_mark=_a] move window to mark _b, [con_mark=_a] focus';
    });

done_testing;
