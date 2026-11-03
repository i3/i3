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
# Verify title borders, padding and unclipped icons at integer and fractional DPI.
# Bug still in: 4.25-25-g9be3249a
use i3test i3_autostart => 0;
use X11::XCB qw(PROP_MODE_REPLACE IMAGE_FORMAT_Z_PIXMAP);
use IPC::Run qw(start timeout);

sub title_pixels {
    my ($connection, $window, $width, $height) = @_;
    my $tree = $connection->query_tree_reply($connection->query_tree($window)->{sequence});
    my $cookie = $connection->get_image(
        IMAGE_FORMAT_Z_PIXMAP, $tree->{parent}, 0, 0, $width, $height, 0xffffffff);
    my $image = $connection->get_image_data_rgba($cookie->{sequence});
    die 'Expected a 24-bit RGB or 32-bit ARGB title image'
        unless ($image->{depth} == 24 || $image->{depth} == 32)
            && length($image->{data}) == 4 * $width * $height;
    return $image->{data};
}

plan skip_all => 'X11::XCB 0.23 or newer is required for pixel capture'
    unless $x->can('get_image_data_rgba');

# The normal suite can use an 8-bit PseudoColor display, which Cairo's XCB
# renderer cannot paint. Give only this pixel test its own TrueColor server.
# -displayfd allocates an unused display and reports it only when ready.
qx(Xvfb -help 2>&1);
plan skip_all => 'Xvfb is required for the TrueColor rendering test' if $?;
my $xserver;
END {
    local $?;
    local $SIG{CHLD};
    $xserver->kill_kill if $xserver;
}
my ($display, $server_errors) = ('', '');
my $ready_timeout = timeout(10);
$xserver = start(
    ['Xvfb', '-displayfd', '1', '-screen', '0', '1280x800x24', '-nolisten', 'tcp', '-noreset'],
    \undef, \$display, \$server_errors, $ready_timeout);
eval { $xserver->pump until $display =~ /^\d+\n$/; };
BAIL_OUT("TrueColor Xvfb failed to start: $@ $server_errors") if $@;
$ready_timeout->reset;
chomp $display;
local $ENV{DISPLAY} = ":$display";
$x = i3test::X11->new;

my $root = $x->get_root_window();
my $resource_atom = $x->atom(name => 'RESOURCE_MANAGER')->id;
my $height_at_96;

for my $dpi (96, 144, 192, 288) {
    my $resources = "Xft.dpi: $dpi\n";
    $x->change_property(PROP_MODE_REPLACE, $root, $resource_atom,
        $x->atom(name => 'STRING')->id, 8, length($resources), $resources);
    $x->flush;
    my $pid = launch_with_config(<<'CONFIG');
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
default_border normal 1
client.focused #ff0000 #263548 #ffffff #ff0000 #ff0000
for_window [class=".*"] title_window_icon padding 3px
CONFIG
    my $workspace = fresh_workspace;
    my $window = open_window(
        name => 'DPI title: Hg / square icon',
        wm_class => 'dpi-test',
        before_map => sub {
            my ($window) = @_;
            my @icon = (64, 64, (0xff00ff00) x (64 * 64));
            $x->change_property(PROP_MODE_REPLACE, $window->id,
                $x->atom(name => '_NET_WM_ICON')->id, $x->atom(name => 'CARDINAL')->id,
                32, scalar(@icon), pack('L*', @icon));
        },
    );
    my ($nodes) = get_ws_content($workspace);
    my $deco = $nodes->[0]->{deco_rect};
    my ($width, $height) = @{$deco}{qw(width height)};
    $height_at_96 = $height if $dpi == 96;
    my $border = int(($dpi + 95) / 96);
    my $padding = int((2 * $dpi + 95) / 96);
    is($height - $height_at_96, 2 * $padding - 4,
        "$dpi DPI: vertical padding scales around the same fixed-pixel font") if $dpi != 96;

    my $pixels = title_pixels($x, $window->id, $width, $height);
    # Measure across each edge, away from corners and title contents.
    for my $edge (
        ['left', 0, int($height / 2), 1, 0],
        ['right', $width - 1, int($height / 2), -1, 0],
        ['top', int($width / 2), 0, 0, 1],
        ['bottom', int($width / 2), $height - 1, 0, -1],
    ) {
        my ($name, $col, $row, $dx, $dy) = @{$edge};
        my $thickness = 0;
        while ($col >= 0 && $col < $width && $row >= 0 && $row < $height
            && substr($pixels, 4 * ($row * $width + $col), 3) eq "\xff\0\0") {
            ++$thickness;
            $col += $dx;
            $row += $dy;
        }
        is($thickness, $border, "$dpi DPI: $name title border has the scaled thickness");
    }

    my ($left, $top, $right, $bottom) = ($width, $height, -1, -1);
    for my $y (0 .. $height - 1) {
        for my $col (0 .. $width - 1) {
            if (substr($pixels, 4 * ($y * $width + $col), 3) eq "\0\xff\0") {
                $left = $col if $col < $left;
                $right = $col if $col > $right;
                $top = $y if $y < $top;
                $bottom = $y if $y > $bottom;
            }
        }
    }
    if (ok($right >= 0, "$dpi DPI: icon is rendered")) {
        is($right - $left, $bottom - $top, "$dpi DPI: square icon is not clipped by the title border");
        is($top, $border, "$dpi DPI: icon clears the top border");
        is($height - 1 - $bottom, $border, "$dpi DPI: icon clears the bottom border equally");
    }
    $window->unmap;
    exit_gracefully($pid);
}

done_testing;
