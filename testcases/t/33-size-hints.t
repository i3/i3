#!perl
# vim:ts=4:sw=4:expandtab
#
# Checks if size hints are interpreted correctly.
#
use i3test;

my $i3 = i3("/tmp/nestedcons");

my $x = X11::XCB::Connection->new;

my $tmp = fresh_workspace;

ok(@{get_ws_content($tmp)} == 0, 'no containers yet');

my $win = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => X11::XCB::Rect->new(x => 0, y => 0, width => 30, height => 30),
    background_color => '#C0C0C0',
);

# XXX: we should check screen size. in screens with an AR of 2.0,
# this is not a good idea.
my $aspect = X11::XCB::Sizehints::Aspect->new;
$aspect->min_num(600);
$aspect->min_den(300);
$aspect->max_num(600);
$aspect->max_den(300);
$win->_create;
$win->map;
sleep 0.25;
$win->hints->aspect($aspect);
$x->flush;

sleep 0.25;

my $rect = $win->rect;
my $ar = $rect->width / $rect->height;
diag("Aspect ratio = $ar");
ok(($ar > 1.90) && ($ar < 2.10), 'Aspect ratio about 2.0');

done_testing;
