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
# Test that splitting and stacked/tabbed layouts do not create redundant
# containers.
# Ticket: #3001
# Bug still in: 4.22-24-ga5da4d54
use i3test;

cmp_tree(
    msg => 'toggling between split h/v',
    layout_before => 'H[a*]',
    layout_after => 'V[a*]',
    cb => sub {
        cmd 'split v, split h, split v';
    });
cmp_tree(
    msg => 'toggling between tabbed/stacked',
    layout_before => 'H[a*]',
    layout_after => 'S[a*]',
    cb => sub {
        cmd 'layout tabbed, layout stacked';
    });
cmp_tree(
    msg => 'split h to v and then tabbed',
    layout_before => 'H[a*]',
    layout_after => 'T[a*]',
    cb => sub {
        cmd 'split v, layout tabbed';
    });
cmp_tree(
    msg => 'repeat tabbed layout',
    layout_before => 'H[a*]',
    layout_after => 'T[a*]',
    cb => sub {
        cmd 'layout tabbed' for 1..5;
    });
cmp_tree(
    msg => 'split v inside tabbed',
    layout_before => 'H[a*]',
    layout_after => 'T[V[a*]]',
    cb => sub {
        cmd 'layout tabbed, split v';
    });
cmp_tree(
    msg => 'split v inside tabbed and back to just tabbed',
    layout_before => 'H[a*]',
    layout_after => 'T[a*]',
    cb => sub {
        cmd 'layout tabbed, split v, layout tabbed';
    });
cmp_tree(
    msg => 'toggle split v inside tabbed',
    layout_before => 'H[a*]',
    layout_after => 'T[V[a*]]',
    cb => sub {
        cmd 'layout tabbed, split v, layout tabbed, split v';
    });
cmp_tree(
    msg => 'tabbed with 2 nodes inside other tabbed',
    layout_before => 'T[a*]',
    layout_after => 'T[T[a b*]]',
    cb => sub {
        cmd 'split v';
        open_window(wm_class => "b", name => "b");
        cmd 'layout tabbed';
    });

done_testing;
