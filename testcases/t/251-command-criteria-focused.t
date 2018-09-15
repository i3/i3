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
# Tests for the special value "__focused__" in command criteria.
# Ticket: #1770
use i3test;
use X11::XCB qw(PROP_MODE_REPLACE);

my ($ws);

sub open_window_with_role {
    my ($role) = @_;
    open_window(
        before_map => sub {
            my ($window) = @_;
            my $atomname = $x->atom(name => 'WM_WINDOW_ROLE');
            my $atomtype = $x->atom(name => 'STRING');
            $x->change_property(
                PROP_MODE_REPLACE,
                $window->id,
                $atomname->id,
                $atomtype->id,
                8,
                length($role) + 1,
                "$role\x00"
            );
        }
    );
}

###############################################################################
# 1: Test __focused__ for window class.
###############################################################################

$ws = fresh_workspace;
open_window(wm_class => 'notme');
open_window(wm_class => 'magic');
open_window(wm_class => 'magic');
is(@{get_ws($ws)->{nodes}}, 3, 'sanity check: workspace contains three windows');

cmd '[class=__focused__] move to workspace trash';
is(@{get_ws($ws)->{nodes}}, 1, '__focused__ works for window class');

###############################################################################
# 2: Test __focused__ for window instance.
###############################################################################

$ws = fresh_workspace;
open_window(instance => 'notme', wm_class => 'test');
open_window(instance => 'magic', wm_class => 'test');
open_window(instance => 'magic', wm_class => 'test');
is(@{get_ws($ws)->{nodes}}, 3, 'sanity check: workspace contains three windows');

cmd '[instance=__focused__] move to workspace trash';
is(@{get_ws($ws)->{nodes}}, 1, '__focused__ works for window instance');

###############################################################################
# 3: Test __focused__ for window title.
###############################################################################

$ws = fresh_workspace;
open_window(name => 'notme');
open_window(name => 'magic');
open_window(name => 'magic');
is(@{get_ws($ws)->{nodes}}, 3, 'sanity check: workspace contains three windows');

cmd '[title=__focused__] move to workspace trash';
is(@{get_ws($ws)->{nodes}}, 1, '__focused__ works for title');

###############################################################################
# 4: Test __focused__ for window role.
###############################################################################

$ws = fresh_workspace;
open_window_with_role("notme");
open_window_with_role("magic");
open_window_with_role("magic");
is(@{get_ws($ws)->{nodes}}, 3, 'sanity check: workspace contains three windows');

cmd '[window_role=__focused__] move to workspace trash';
is(@{get_ws($ws)->{nodes}}, 1, '__focused__ works for window_role');

###############################################################################
# 5: Test __focused__ for workspace.
###############################################################################

$ws = fresh_workspace;
open_window;
open_window;
is(@{get_ws($ws)->{nodes}}, 2, 'sanity check: workspace contains two windows');

cmd '[workspace=__focused__] move to workspace trash';
is(@{get_ws($ws)->{nodes}}, 0, '__focused__ works for workspace');

###############################################################################
# 6: Test that __focused__ in command criteria when no window is focused does
# not crash i3.
# See issue: #3406
###############################################################################

fresh_workspace;
cmd '[class=__focused__] focus';
does_i3_live;

###############################################################################

done_testing;
