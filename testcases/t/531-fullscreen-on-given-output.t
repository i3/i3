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
# Tests that fullscreen windows appear on the output indicated by
# their geometry
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT
use List::Util qw(first);

# Helper functions
sub fullscreen($) {
    my ($window) = @_;
    $window->fullscreen(1);
}

sub find_window {
    my ($nodes, $id) = @_;

    foreach (@{$nodes}) {
	return $_ if ($_->{window} // 0) == $id;
	my $node = find_window($_->{nodes}, $id);
	return $node if $node;
    };
    return undef;
}

# Create two fullscreen windows, each on different output
my $orig_rect1 = X11::XCB::Rect->new(x => 0, y => 0, width => 1024, height => 768);
my $orig_rect2 = X11::XCB::Rect->new(x => 1024, y => 0, width => 1024, height => 768);

my $win_on_first_output = open_window(rect => $orig_rect1,
				      before_map => \&fullscreen);

my $win_on_second_output = open_window(rect => $orig_rect2,
				       before_map => \&fullscreen);

sync_with_i3;

# Check that the windows are on the correct output
is_deeply(scalar $win_on_first_output->rect, $orig_rect1, "first window spans the first output");
is_deeply(scalar $win_on_second_output->rect, $orig_rect2, "second window spans the second output");

# Check that both windows remained fullscreen
my $tree = i3(get_socket_path())->get_tree->recv;

my $node1 = find_window($tree->{nodes}, $win_on_first_output->{id});
my $node2 = find_window($tree->{nodes}, $win_on_second_output->{id});

is($node1->{fullscreen_mode}, 1, "first window is fullscreen");
is($node2->{fullscreen_mode}, 1, "second window is fullscreen");

done_testing;
