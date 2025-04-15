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
# Checks if size hints are interpreted correctly.
#
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

default_floating_border none
floating_minimum_size -1 x -1
floating_maximum_size -1 x -1
EOT

sub open_with_aspect {
    my ($min_num, $min_den, $max_num, $max_den) = @_;
    open_floating_window(
        rect => [0, 0, 100, 100],
        before_map => sub {
            my ($window) = @_;
            my $aspect = X11::XCB::Sizehints::Aspect->new;
            $aspect->min_num($min_num);
            $aspect->min_den($min_den);
            $aspect->max_num($max_num);
            $aspect->max_den($max_den);
            $window->hints->aspect($aspect);
        });
}

################################################################################
# Test aspect ratio set exactly to 2.0
################################################################################

fresh_workspace;
my $win = open_with_aspect(600, 300, 600, 300);

my $rect = $win->rect;
my $ar = $rect->width / $rect->height;
cmp_float($ar, 2, 'Window set to floating with aspect ratio 2.0');

cmd 'resize set 100';
$rect = $win->rect;
$ar = $rect->width / $rect->height;
cmp_float($ar, 2, 'Window resized with aspect ratio kept to 2.0');

cmd 'resize set 400 100';
$rect = $win->rect;
$ar = $rect->width / $rect->height;
cmp_float($ar, 2, 'Window resized with aspect ratio kept to 2.0');

# Also check that it is possible to resize by height only
cmd 'resize set height 400';
$rect = $win->rect;
$ar = $rect->width / $rect->height;
is($rect->height, 400, 'Window height is 400px');
cmp_float($ar, 2, 'Window resized with aspect ratio kept to 2.0');

cmd 'resize grow height 10';
$rect = $win->rect;
$ar = $rect->width / $rect->height;
is($rect->height, 410, 'Window grew by 10px');
cmp_float($ar, 2, 'Window resized with aspect ratio kept to 2.0');

################################################################################
# Test aspect ratio between 0.5 and 2.0
################################################################################

fresh_workspace;
$win = open_with_aspect(1, 2, 2, 1);

$rect = $win->rect;
$ar = $rect->width / $rect->height;
cmp_float($ar, 1, 'Window set to floating with aspect ratio 1.0');

cmd 'resize set 200';
$rect = $win->rect;
$ar = $rect->width / $rect->height;
is($rect->width, 200, 'Window width is 200px');
is($rect->height, 100, 'Window height stayed 100px');
cmp_float($ar, 2, 'Window resized, aspect ratio changed to 2.0');

cmd 'resize set 100 200';
$rect = $win->rect;
$ar = $rect->width / $rect->height;
is($rect->width, 100, 'Window width is 100px');
is($rect->height, 200, 'Window height is 200px');
cmp_float($ar, 0.5, 'Window resized, aspect ratio changed to 0.5');

cmd 'resize set 500';
$rect = $win->rect;
$ar = $rect->width / $rect->height;
cmp_float($ar, 2, 'Window resized, aspect ratio changed to maximum 2.0');

cmd 'resize set 100 400';
$rect = $win->rect;
$ar = $rect->width / $rect->height;
cmp_float($ar, 0.5, 'Window resized, aspect ratio changed to minimum 0.5');

done_testing;
