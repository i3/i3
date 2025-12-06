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
# Test that marks on a parent container survive restart
#
use i3test;
use v5.10;

sub get_marks {
    return i3(get_socket_path())->get_marks->recv;
}

my $left = open_window;
my $top = open_window;
cmd 'split v';
my $bottom = open_window;

cmd 'mark bottom';
cmd 'focus up';
cmd 'mark top';
cmd 'focus parent';
cmd 'mark right';
cmd 'focus left';
cmd 'mark left';

is_deeply([sort(@{get_marks()})], ["bottom", "left", "right", "top"], 'all four marks set');

cmd 'restart';

is_deeply([sort(@{get_marks()})], ["bottom", "left", "right", "top"], 'all four marks survive');

done_testing;
