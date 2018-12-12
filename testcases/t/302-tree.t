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
# Contains various tests that use the cmp_tree subroutine.
# Ticket: #3503
use i3test;

sub sanity_check {
    local $Test::Builder::Level = $Test::Builder::Level + 1;

    my ($layout, $focus_idx) = @_;
    my @windows = cmp_tree(
        msg => 'Sanity check',
        layout_before => $layout,
        layout_after => $layout);
    is($x->input_focus, $windows[$focus_idx]->id, 'Correct window focused') if $focus_idx >= 0;
}

sanity_check('H[ V[ a* V[ b c ] d ] e ]', 0);
sanity_check('H[ a b c d* ]', 3);
sanity_check('V[a b] V[c d*]', 3);
sanity_check('T[a b] S[c*]', 2);

cmp_tree(
    msg => 'Simple focus test',
    layout_before => 'H[a b] V[c* d]',
    layout_after => 'H[a* b] V[c d]',
    cb => sub {
        cmd '[class=a] focus';
    });

cmp_tree(
    msg => 'Simple move test',
    layout_before => 'H[a b] V[c* d]',
    layout_after => 'H[a b] V[d c*]',
    cb => sub {
        cmd 'move down';
    });

cmp_tree(
    msg => 'Move from horizontal to vertical',
    layout_before => 'H[a b] V[c d*]',
    layout_after => 'H[b] V[c d a*]',
    cb => sub {
        cmd '[class=a] focus';
        cmd 'move right, move right';
    });

cmp_tree(
    msg => 'Move unfocused non-leaf container',
    layout_before => 'S[a b] V[c d* T[e f g]]',
    layout_after => 'S[a T[e f g] b] V[c d*]',
    cb => sub {
        cmd '[con_mark=T1] move up, move up, move left, move up';
    });

cmp_tree(
    msg => 'Simple swap test',
    layout_before => 'H[a b] V[c d*]',
    layout_after => 'H[a d*] V[c b]',
    cb => sub {
        cmd '[class=b] swap with id ' . $_[0][3]->{id};
    });

cmp_tree(
    msg => 'Swap non-leaf containers',
    layout_before => 'S[a b] V[c d*]',
    layout_after => 'V[c d*] S[a b]',
    cb => sub {
        cmd '[con_mark=S1] swap with mark V1';
    });

cmp_tree(
    msg => 'Swap nested non-leaf containers',
    layout_before => 'S[a b] V[c d* T[e f g]]',
    layout_after => 'T[e f g] V[c d* S[a b]]',
    cb => sub {
        cmd '[con_mark=S1] swap with mark T1';
    });

done_testing;
