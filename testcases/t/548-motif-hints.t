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
# Test that setting and unsetting motif hints updates window decorations
# accordingly, respecting user configuration.
# Ticket: #3678
# Ticket: #5149
# Ticket: #5438
# Bug still in: 4.21
use File::Temp qw(tempfile);
use List::Util qw(first);
use X11::XCB qw(:all);
use i3test i3_autostart => 0;

my $use_floating;
sub subtest_with_config {
    my ($style, $cb) = @_;
    my $some_other_style = $style eq "normal" ? "pixel" : "normal";

    subtest 'with tiling', sub {
    my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

default_border $style
default_floating_border $some_other_style
EOT
    my $pid = launch_with_config($config);
    $use_floating = 0;
    $cb->();
    exit_gracefully($pid);
    };

    subtest 'with floating', sub {
    my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

default_border $some_other_style
default_floating_border $style
EOT
    my $pid = launch_with_config($config);
    $use_floating = 1;
    $cb->();
    exit_gracefully($pid);
    };
}

sub _change_motif_property {
    my ($window, $value) = @_;
    $x->change_property(
        PROP_MODE_REPLACE,
        $window->id,
        $x->atom(name => '_MOTIF_WM_HINTS')->id,
        $x->atom(name => 'CARDINAL')->id,
        32, 5,
        pack('L5', 2, 0, $value, 0, 0),
    );
    $x->flush;
}

sub open_window_with_motifs {
    my ($value, %args) = @_;

    $args{kill_all} //= 1;

    # we don't need other windows anymore, simplifies get_border_style
    kill_all_windows if $args{kill_all};

    my $open = \&open_window;
    if ($use_floating) {
        $open = \&open_floating_window;
    }

    my $window = $open->(
        %args,
        before_map => sub {
            my ($window) = @_;
            _change_motif_property($window, $value);
        },
    );

    sync_with_i3;

    return $window;
}

my $window;
sub change_motif_property {
    my $value = shift;
    _change_motif_property($window, $value);
    sync_with_i3;
}

sub get_border_style {
    if ($use_floating) {
        my @floating = @{get_ws(focused_ws)->{floating_nodes}};
        return $floating[0]->{nodes}[0]->{border};
    }

    return @{get_ws(focused_ws)->{nodes}}[0]->{border};
}

sub is_border_style {
    my ($expected, $extra_msg) = @_;
    my $msg = "border style $expected";
    if (defined $extra_msg) {
        $msg = "$msg: $extra_msg";
    }

    local $Test::Builder::Level = $Test::Builder::Level + 1;
    is(get_border_style, $expected, $msg);
}

###############################################################################
subtest 'with default_border normal', \&subtest_with_config, 'normal',
sub {
$window = open_window_with_motifs(0);
is_border_style('none');

$window = open_window_with_motifs(1 << 0);
is_border_style('normal');

$window = open_window_with_motifs(1 << 1);
is_border_style('pixel');

$window = open_window_with_motifs(1 << 3);
is_border_style('normal');

cmd 'border pixel';
is_border_style('pixel', 'set by user');

change_motif_property(0);
is_border_style('none');

change_motif_property(1);
is_border_style('pixel', 'because of user maximum=pixel');

cmd 'border none';
is_border_style('none', 'set by user');

change_motif_property(0);
is_border_style('none');

change_motif_property(1);
is_border_style('none', 'because of user maximum=none');
};

subtest 'with default_border pixel', \&subtest_with_config, 'pixel',
sub {
$window = open_window_with_motifs(0);
is_border_style('none');

$window = open_window_with_motifs(1 << 0);
is_border_style('pixel');

$window = open_window_with_motifs(1 << 1);
is_border_style('pixel');

$window = open_window_with_motifs(1 << 3);
is_border_style('pixel');

cmd 'border normal';
is_border_style('normal', 'set by user');

change_motif_property(0);
is_border_style('none');

change_motif_property(1);
is_border_style('normal', 'because of user maximum=normal');
};

subtest 'with default_border none', \&subtest_with_config, 'none',
sub {
$window = open_window_with_motifs(0);
is_border_style('none');

$window = open_window_with_motifs(1 << 0);
is_border_style('none');

$window = open_window_with_motifs(1 << 1);
is_border_style('none');

$window = open_window_with_motifs(1 << 3);
is_border_style('none');

cmd 'border pixel';
is_border_style('pixel', 'set by user');

change_motif_property(0);
is_border_style('none');

change_motif_property(1);
is_border_style('pixel', 'because of user maximum=pixel');
};

###############################################################################
# Test with append_layout
# See #5438
###############################################################################

$use_floating = 0;

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

EOT
my $pid = launch_with_config($config);

my ($fh, $filename) = tempfile(UNLINK => 1);
print $fh <<'EOT';
{
	"nodes": [
		{
			"border": "none",
			"swallows": [
				{
					"class": "^Special$"
				}
			],
			"type": "con"
		}
	],
	"type": "con"
}
EOT
$fh->flush;
cmd "append_layout $filename";

# can't use get_border_style because append_layout creates a parent container
is(@{get_ws(focused_ws)->{nodes}}[0]->{nodes}[0]->{border}, 'none', 'placeholder has border style none');

$window = open_window_with_motifs(1, wm_class => 'Special', instance => 'Special', kill_all => 0);
is(@{get_ws(focused_ws)->{nodes}}[0]->{nodes}[0]->{border}, 'none', 'window has border style none');

done_testing;
