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
# Tests the 'focus local|global <n>' directives.
# Ticket: #1214
use i3test;

sub cmd_focus {
    my ($type, $n) = @_;
    my $command = cmd "focus $type $n";
    sync_with_i3;
    return $command->[0];
}

my ($A, $B, $C, $D, $command);
my @types = ('local', 'global');

###############################################################################
# Given three containers A, B and C in horizontal|vertical layout, when
# focusing local|global, then the correct container is selected.
###############################################################################

for my $layout (('horizontal', 'vertical')) {

    fresh_workspace;
    cmd "layout $layout";
    $A = open_window;
    $B = open_window;
    $C = open_window;

    for my $type (@types) {

        cmd_focus('local', 1);
        is($x->input_focus, $A->{id}, 'A is focused');
        cmd_focus('local', 2);
        is($x->input_focus, $B->{id}, 'B is focused');
        cmd_focus('local', 3);
        is($x->input_focus, $C->{id}, 'C is focused');

    }

}

###############################################################################
# Given a tabbed container with tabs A, B and C, when focusing local, then
# the correct tab is selected.
# Given the same tabbed container, when focusing global, then only '1' works.
###############################################################################

fresh_workspace;
$A = open_window;
cmd 'layout tabbed';
$B = open_window;
$C = open_window;

cmd_focus('local', 1);
is($x->input_focus, $A->{id}, 'A is focused when selecting 1 locally');
cmd_focus('local', 2);
is($x->input_focus, $B->{id}, 'B is focused when selecting 2 locally');

cmd_focus('global', 1);
is($x->input_focus, $B->{id}, 'B is focused when selecting 1 globally');

$command = cmd_focus('global', 2);
ok(!$command->{success}, 'command fails when selecting 2 globally');
is($x->input_focus, $B->{id}, 'B is still focused');

###############################################################################
# Given H[A V[B C]] as a layout, when selecting globally, then the correct
# window is focused.
###############################################################################

fresh_workspace;
$A = open_window;
$B = open_window;
cmd 'split v';
$C = open_window;

cmd_focus('global', 1);
is($x->input_focus, $A->{id}, 'A is focused when selecting 1 globally');
cmd_focus('global', 2);
is($x->input_focus, $B->{id}, 'B is focused when selecting 2 globally');
cmd_focus('global', 3);
is($x->input_focus, $C->{id}, 'C is focused when selecting 3 globally');

###############################################################################
# Given H[A V[B C]] as a layout, when selecting locally, then the correct
# window is focused.
###############################################################################

fresh_workspace;
$A = open_window;
$B = open_window;
cmd 'split v';
$C = open_window;

cmd_focus('local', 1);
is($x->input_focus, $B->{id}, 'B is focused when selecting 1 locally');
cmd_focus('local', 2);
is($x->input_focus, $C->{id}, 'C is focused when selecting 2 locally');
$command = cmd_focus('local', 3);
ok(!$command->{success}, 'command fails when selecting 3 locally');

###############################################################################
# Given H[A V[B S[C D]]], when focusing locally, then the correct focus is
# set.
# Given the same layout, when focusing the stacked container globally, then
# the focus head is selected.
###############################################################################

fresh_workspace;
$A = open_window;
$B = open_window;
cmd 'split v';
$C = open_window;
cmd 'split v';
$D = open_window;
cmd 'layout stacked';

cmd_focus('local', 1);
is($x->input_focus, $C->{id}, 'C is focused when selecting 1 locally');

cmd 'focus parent';
cmd_focus('local', 1);
is($x->input_focus, $B->{id}, 'B is focused when selecting 1 locally');

cmd 'focus left';
cmd_focus('global', 3);
is($x->input_focus, $C->{id}, 'C is focused when selecting 3 globally');

###############################################################################
# Given nested tabbed containers, when selecting locally, then the correct
# tab is selected.
# Given the same layout, when focusing parents, then the correct tabbed
# container is used and the focus head of its tab is selected.
###############################################################################

fresh_workspace;
$A = open_window;
cmd 'layout tabbed';
$B = open_window;
cmd 'split v';
$C = open_window;
cmd 'layout stacked';
$D = open_window;

cmd_focus('local', 2);
is($x->input_focus, $C->{id}, 'C is focused when selecting 2 locally');

cmd 'focus parent';
cmd_focus('local', 1);
is($x->input_focus, $A->{id}, 'A is focused when selecting 1 locally');

###############################################################################

done_testing;
