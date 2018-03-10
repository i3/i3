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
# Tests that focus does not wrap when focus_wrapping is disabled in
# the configuration.
# Ticket: #2352
# Bug still in: 4.14-72-g6411130c
use i3test i3_config => <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

focus_wrapping no
EOT

sub test_orientation {
    my ($orientation, $prev, $next) = @_;
    my $tmp = fresh_workspace;

    cmd "split $orientation";

    my $win1 = open_window;
    my $win2 = open_window;

    is($x->input_focus, $win2->id, "Second window focused initially");
    cmd "focus $prev";
    is($x->input_focus, $win1->id, "First window focused");
    cmd "focus $prev";
    is($x->input_focus, $win1->id, "First window still focused");
    cmd "focus $next";
    is($x->input_focus, $win2->id, "Second window focused");
    cmd "focus $next";
    is($x->input_focus, $win2->id, "Second window still focused");
}

test_orientation('v', 'up', 'down');
test_orientation('h', 'left', 'right');

done_testing;
