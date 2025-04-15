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
# TODO: Description of this file.
# Ticket: #999
# Bug still in: 4.13-12-g2ff3d9d
use File::Temp qw(tempfile);
use i3test i3_autostart => 0;

my $monitor_name = 'i3-fake-monitor';
my $output_name = 'i3-fake-output';

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

bar {
    output $output_name
}
EOT

my ($outfh, $outname) = tempfile('i3-randr15reply-XXXXXX', UNLINK => 1);

# Prepare a RRGetMonitors reply, see A.2.4 in
# https://cgit.freedesktop.org/xorg/proto/randrproto/tree/randrproto.txt
my $reply = pack('cxSLLLLx[LLL]',
     1, # reply
     0, # sequence (will be filled in by inject_randr15)
     # 60 = length($reply) + length($monitor1)
     # 32 = minimum X11 reply length
     (60-32) / 4, # length in words
     0, # timestamp TODO
     1, # nmonitors
     1); # noutputs

# Manually intern _NET_CURRENT_DESKTOP as $x->atom will not create atoms if
# they are not yet interned.
my $atom_cookie = $x->intern_atom(0, length($monitor_name), $monitor_name);
my $monitor_name_atom = $x->intern_atom_reply($atom_cookie->{sequence})->{atom};

# MONITORINFO is defined in A.1.1 in
# https://cgit.freedesktop.org/xorg/proto/randrproto/tree/randrproto.txt
my $monitor1 = pack('LccSssSSLLL',
        $monitor_name_atom, # name (ATOM)
        1, # primary
        1, # automatic
        1, # ncrtcs
        0, # x
        0, # y
        3840, # width in pixels
        2160, # height in pixels
        520, # width in millimeters
        290, # height in millimeters
        12345); # output ID #0

print $outfh $reply;
print $outfh $monitor1;

close($outfh);

# Prepare a RRGetOutputInfo reply as well; see RRGetOutputInfo in
# https://www.x.org/releases/current/doc/randrproto/randrproto.txt
($outfh, my $outname_moninfo) = tempfile('i3-randr15reply-XXXXXX', UNLINK => 1);
my $moninfo = pack('cxSLLLx[LLccSSSS]S a* x!4',
                   1, # reply
                   0, # sequence (will be filled in by inject_randr15)
                   # 36 = length($moninfo) (without name and padding)
                   # 32 = minimum X11 reply length
                   ((36 + length($output_name) - 32) + 3) / 4, # length in words
                   0, # timestamp TODO
                   12345, # CRTC
                   length($output_name), # length of name
                   $output_name); # name

print $outfh $moninfo;
close($outfh);

my $pid = launch_with_config($config,
                             inject_randr15 => $outname,
                             inject_randr15_outputinfo => $outname_moninfo);

my $tree = i3->get_tree->recv;
my @outputs = map { $_->{name} } @{$tree->{nodes}};
is_deeply(\@outputs, [ '__i3', $monitor_name ], 'outputs are __i3 and the fake monitor');

my ($output_data) = grep { $_->{name} eq $monitor_name } @{$tree->{nodes}};
is_deeply($output_data->{rect}, {
        width => 3840,
        height => 2160,
        x => 0,
        y => 0,
    }, "Fake output at 3840x2160+0+0");

# Verify that i3 canonicalizes RandR output names to i3 output names
# (RandR monitor names) for bar configs

my $bars = i3->get_bar_config()->recv;
is(@$bars, 1, 'one bar configured');

my $bar_id = shift @$bars;

my $bar_config = i3->get_bar_config($bar_id)->recv;
is_deeply($bar_config->{outputs}, [ $monitor_name ], 'bar_config output name is normalized');

exit_gracefully($pid);

################################################################################
# Verify that adding monitors with RandR 1.5 results in i3 outputs.
################################################################################

# When inject_randr15 is defined but false, fake-xinerama will be turned off,
# but inject_randr15 will not actually be used.
$pid = launch_with_config($config, inject_randr15 => '');

$tree = i3->get_tree->recv;
@outputs = map { $_->{name} } @{$tree->{nodes}};
is_deeply(\@outputs, [ '__i3', 'default' ], 'outputs are __i3 and default');

SKIP: {
    my @events = events_for(
        sub {
            skip 'xrandr --setmonitor failed (xrandr too old?)', 1
              unless system(q|xrandr --setmonitor up2414q 3840/527x2160/296+1280+0 none|) == 0;
        },
        "workspace");

    my @init = grep { $_->{change} eq 'init' } @events;
    is(scalar @init, 1, 'Received 1 workspace::init event');
    is($init[0]->{current}->{output}, 'up2414q', 'Workspace initialized in up2414q');

    $tree = i3->get_tree->recv;
    @outputs = map { $_->{name} } @{$tree->{nodes}};
    is_deeply(\@outputs, [ '__i3', 'default', 'up2414q' ], 'outputs are __i3, default and up2414q');
}

exit_gracefully($pid);

done_testing;
