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
# Verify that i3 does not crash when command criteria that match a scratchpad
# window are used with the focus output command or other commands
# Ticket: #6076
# Bug still in: 4.23-43-g822477cb
use i3test;

my $ws = fresh_workspace;
my $win = open_window;
cmd "move scratchpad";

sub cmd_on_w {
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    my $c = shift;
    subtest "$c" => sub {
        my $result = cmd '[id="' . $win->id . '"] ' . $c;
        is($result->[0]->{success}, 1, "command succeeded");
        is(@{get_ws($ws)->{floating_nodes}}, 0, 'no floating windows on workspace');
        is(@{get_ws($ws)->{nodes}}, 0, 'no nodes on workspace');
    }
}

cmd_on_w 'nop';
cmd_on_w 'focus output left';
cmd_on_w 'focus left';
cmd_on_w 'floating disable';
cmd_on_w 'floating enable';

does_i3_live;

done_testing;
