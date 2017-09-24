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
# Checks if size hints are interpreted correctly.
#
use i3test;

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $win = open_window({ dont_map => 1 });
# XXX: we should check screen size. in screens with an AR of 2.0,
# this is not a good idea.
my $aspect = X11::XCB::Sizehints::Aspect->new;
$aspect->min_num(600);
$aspect->min_den(300);
$aspect->max_num(600);
$aspect->max_den(300);
$win->_create;
$win->map;
wait_for_map $win;
$win->hints->aspect($aspect);
$x->flush;

sync_with_i3;

my $rect = $win->rect;
my $ar = $rect->width / $rect->height;
diag("Aspect ratio = $ar");
ok(($ar > 1.90) && ($ar < 2.10), 'Aspect ratio about 2.0');

done_testing;
