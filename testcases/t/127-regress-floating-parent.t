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
# Regression: make a container floating, kill its parent, make it tiling again
#
use i3test;

my $tmp = fresh_workspace;

cmd 'open';
my $left = get_focused($tmp);
cmd 'open';
my $old = get_focused($tmp);
cmd 'split v';
cmd 'open';
my $floating = get_focused($tmp);
diag("focused floating: " . get_focused($tmp));
cmd 'mode toggle';
sync_with_i3;

# kill old container
cmd qq|[con_id="$old"] focus|;
is(get_focused($tmp), $old, 'old container focused');
cmd 'kill';

# kill left container
cmd qq|[con_id="$left"] focus|;
is(get_focused($tmp), $left, 'old container focused');
cmd 'kill';

# focus floating window, make it tiling again
cmd qq|[con_id="$floating"] focus|;
is(get_focused($tmp), $floating, 'floating window focused');

sync_with_i3;
cmd 'mode toggle';

does_i3_live;

done_testing;
