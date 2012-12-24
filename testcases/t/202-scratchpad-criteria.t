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
# Verifies that using criteria to address scratchpad windows works.
use i3test;

################################################################################
# Verify that using scratchpad show with criteria works as expected:
# When matching a scratchpad window which is visible, it should hide it.
# When matching a scratchpad window which is on __i3_scratch, it should show it.
# When matching a non-scratchpad window, it should be a no-op.
################################################################################

my $tmp = fresh_workspace;

my $third_window = open_window(name => 'scratch-match');
cmd 'move scratchpad';

# Verify that using 'scratchpad show' without any matching windows is a no-op.
my $old_focus = get_focused($tmp);

cmd '[title="nomatch"] scratchpad show';

is(get_focused($tmp), $old_focus, 'non-matching criteria have no effect');

# Verify that we can use criteria to show a scratchpad window.
cmd '[title="scratch-match"] scratchpad show';

my $scratch_focus = get_focused($tmp);
isnt($scratch_focus, $old_focus, 'matching criteria works');

cmd '[title="scratch-match"] scratchpad show';

isnt(get_focused($tmp), $scratch_focus, 'matching criteria works');
is(get_focused($tmp), $old_focus, 'focus restored');

# Verify that we cannot use criteria to show a non-scratchpad window.
my $tmp2 = fresh_workspace;
my $non_scratch_window = open_window(name => 'non-scratch');
cmd "workspace $tmp";
is(get_focused($tmp), $old_focus, 'focus still ok');
cmd '[title="non-match"] scratchpad show';
is(get_focused($tmp), $old_focus, 'focus unchanged');

done_testing;
