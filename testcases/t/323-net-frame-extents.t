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
# Verify _NET_FRAME_EXTENTS is set correctly
# Ticket: #4292
# Bug still in: 4.24-8-g44b67d11
use i3test i3_autostart => 0;
use X11::XCB qw(:all);

my $pid = launch_with_config('-default');

sub net_frame_extents {
    my ($window) = @_;

    my $cookie = $x->get_property(
        0,
        $window->{id},
        $x->atom(name => '_NET_FRAME_EXTENTS')->id,
        GET_PROPERTY_TYPE_ANY,
        0,
        4
    );

    my $reply = $x->get_property_reply($cookie->{sequence});
    my $len = $reply->{length};
    return [] if $len == 0;

    return unpack("L$len", $reply->{value});
}

sub is_net_frame_extents {
    my ($window, $expect, $msg) = @_;
    $msg //= "";
    $msg = "frame extents $msg";
    $msg =~ s/\s+$//;
    my @extents = net_frame_extents($window);
    is_deeply(\@extents, $expect, "$msg: got: @extents want: @$expect");
}

subtest 'basic border styles' => sub {
    my $w = open_window;
    cmd 'border normal 3';
    is_net_frame_extents($w, [3, 3, 18, 3], "normal border with 3px width");

    cmd 'border pixel 1';
    is_net_frame_extents($w, [1, 1, 1, 1], "pixel border with 1px width");

    cmd 'border pixel 5';
    is_net_frame_extents($w, [5, 5, 5, 5], "pixel border with 5px width");

    open_window;
    is_net_frame_extents($w, [5, 5, 5, 5], "other window does not affect");
    cmd 'kill';

    cmd 'border normal 0';
    is_net_frame_extents($w, [0, 0, 18, 0], "normal border with 0px width");

    cmd 'border none';
    is_net_frame_extents($w, [0, 0, 0, 0], "no border");
};

subtest 'multiple windows in different layouts' => sub {
    fresh_workspace;
    
    my $w1 = open_window;
    my $w2 = open_window;
    my $w3 = open_window;
    
    cmd 'border normal 2';
    is_net_frame_extents($w1, [2, 2, 18, 2], "window 1 in splith layout with normal border");
    is_net_frame_extents($w2, [2, 2, 18, 2], "window 2 in splith layout with normal border");
    is_net_frame_extents($w3, [2, 2, 18, 2], "window 3 in splith layout with normal border");
    
    cmd 'layout stacking';
    is_net_frame_extents($w1, [2, 2, 0, 2], "window 1 in stacking layout");
    is_net_frame_extents($w2, [2, 2, 0, 2], "window 2 in stacking layout");
    is_net_frame_extents($w3, [2, 2, 0, 2], "window 3 in stacking layout");
    
    cmd 'layout tabbed';
    is_net_frame_extents($w1, [2, 2, 0, 2], "window 1 in tabbed layout");
    is_net_frame_extents($w2, [2, 2, 0, 2], "window 2 in tabbed layout");
    is_net_frame_extents($w3, [2, 2, 0, 2], "window 3 in tabbed layout");
    
    cmd 'layout splitv';
    is_net_frame_extents($w1, [2, 2, 18, 2], "window 1 in splitv layout");
    is_net_frame_extents($w2, [2, 2, 18, 2], "window 2 in splitv layout");
    is_net_frame_extents($w3, [2, 2, 18, 2], "window 3 in splitv layout");
};

sub launch_with_hide_edge_borders {
    my ($value) = @_;
    my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
hide_edge_borders $value
EOT

    kill_all_windows;
    exit_gracefully($pid);
    $pid = launch_with_config($config);
}

subtest 'hide_edge_borders' => sub {
    launch_with_hide_edge_borders('none');
    my $w = open_window;
    cmd 'border normal 3';
    is_net_frame_extents($w, [3, 3, 18, 3], "window with normal borders (hide_edge_borders none)");

    launch_with_hide_edge_borders('vertical');
    $w = open_window;
    cmd 'border normal 3';
    is_net_frame_extents($w, [0, 0, 18, 3], "window with hidden vertical borders");

    launch_with_hide_edge_borders('horizontal');
    $w = open_window;
    cmd 'border normal 3';
    is_net_frame_extents($w, [3, 3, 18, 0], "window with hidden horizontal borders");
    cmd 'border pixel 3';
    is_net_frame_extents($w, [3, 3, 0, 0], "window with hidden horizontal borders");

    launch_with_hide_edge_borders('both');
    $w = open_window;
    cmd 'border normal 3';
    is_net_frame_extents($w, [0, 0, 18, 0], "window with all edge borders hidden");
    cmd 'border pixel 3';
    is_net_frame_extents($w, [0, 0, 0, 0], "window with all edge borders hidden");

    launch_with_hide_edge_borders('smart');
    $w = open_window;
    cmd 'border normal 3';
    is_net_frame_extents($w, [0, 0, 18, 0], "window with smart borders (single window)");
    cmd 'border pixel 3';
    is_net_frame_extents($w, [0, 0, 0, 0], "window with smart borders (single window)");
    
    my $w2 = open_window;
    cmd 'border normal 5';
    is_net_frame_extents($w, [3, 3, 3, 3], "first window with smart borders (multiple windows)");
    is_net_frame_extents($w2, [5, 5, 18, 5], "second window with smart borders (multiple windows)");

    exit_gracefully($pid);
    launch_with_config('-default');
};

subtest 'floating windows' => sub {
    fresh_workspace;
    my $w = open_window;
    cmd 'border normal 4';
    is_net_frame_extents($w, [4, 4, 18, 4], "tiling window with normal border");
    
    cmd 'floating enable';
    is_net_frame_extents($w, [4, 4, 18, 4], "floating window with normal border");
    
    cmd 'border pixel 2';
    is_net_frame_extents($w, [2, 2, 2, 2], "floating window with pixel border");
    
    cmd 'border none';
    is_net_frame_extents($w, [0, 0, 0, 0], "floating window with no border");
};

done_testing;
